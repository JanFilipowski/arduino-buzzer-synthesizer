#include "player.h"
#include "logger.h"  

// FIFO na nuty
static NoteEvent nextEvent;
static bool      nextLoaded;
static NoteEvent activeEvents[MAX_ACTIVE_EVENTS];
static uint8_t   activeCount;

static const int buzzerPins[NUM_BUZZERS] = {28, 29, 30, 31, 32};
static Tone      buzzers[NUM_BUZZERS];
static bool initiated = 0;
static double tempoFactor    = 1.0;
static double transposeFactor = 1.0;
static int    transposeSemitones = 0;
static const char* currentFile = nullptr;

void player_init(void) {
    if (!initiated) {
        for (uint8_t i = 0; i < NUM_BUZZERS; i++) {
            buzzers[i].begin(buzzerPins[i]);
        }
        initiated = true;
    }
    // przywróć domyślne wartości
    tempoFactor     = 1.0;
    transposeFactor = 1.0;
    activeCount     = 0;
    nextLoaded      = sd_read_next_event(&nextEvent);
    currentFile = nullptr;
}

void player_stop_all(void) {
    for (uint8_t i = 0; i < activeCount; i++) {
        int idx = activeEvents[i].buzzer - 1;
        if (idx >= 0 && idx < NUM_BUZZERS) {
            buzzers[idx].stop();
        }
    }
    activeCount = 0;
    interrupts();
}

void player_modify_transpose(int semitones) {
    transposeSemitones += semitones;
    transposeFactor = pow(2.0, transposeSemitones / 12.0);
}
void player_update(unsigned long currentTime) {
    interrupts();

    // start nowych nut (skalujemy czas - prędkość)
    while (nextLoaded && nextEvent.startTime <= currentTime * tempoFactor) {
        int idx = nextEvent.buzzer - 1;
        if (idx >= 0 && idx < NUM_BUZZERS) {
            // skalujemy częstotliwość (transpozycja)
            uint16_t freq = (uint16_t)(nextEvent.frequency * transposeFactor);
            buzzers[idx].play(freq);
            if (activeCount < MAX_ACTIVE_EVENTS) {
                activeEvents[activeCount++] = nextEvent;
            }
        }
        nextLoaded = sd_read_next_event(&nextEvent);
    }

    // zatrzymujemy zakończone nuty (również skalujemy czas - prędkość)
    for (uint8_t i = 0; i < activeCount; ) {
        if (activeEvents[i].endTime <= currentTime * tempoFactor) {
            int idx = activeEvents[i].buzzer - 1;
            if (idx >= 0 && idx < NUM_BUZZERS) {
                buzzers[idx].stop();
            }
            // usuń z kolejki
            for (uint8_t j = i; j + 1 < activeCount; j++) {
                activeEvents[j] = activeEvents[j + 1];
            }
            activeCount--;
        } else {
            i++;
        }
    }
}

bool player_is_idle(void) {
    return (!nextLoaded && activeCount == 0);
}


// player_seek - umożliwia odtworzenie utworu od dowolnego momentu - używane podczas opcji Rewind
void player_seek(unsigned long newTime, const char* filename) {

    // 1) Zatrzymaj wszystkie buzzery
    player_stop_all();
    currentFile = filename;
    sd_open_file(currentFile);

    activeCount = 0;
    nextLoaded  = sd_read_next_event(&nextEvent);

    // 4) Odczytuj kolejne nuty aż do newTime
    NoteEvent e;
    while (nextLoaded && nextEvent.startTime <= newTime) {
        // jeżeli nuta ciągle trwa w punkcie newTime, włącz ją
        if (nextEvent.endTime > newTime) {
            int idx = nextEvent.buzzer - 1;
            if (idx>=0 && idx<NUM_BUZZERS) {
                buzzers[idx].play((uint16_t)(nextEvent.frequency * transposeFactor));
                activeEvents[activeCount++] = nextEvent;
            }
        }

        nextLoaded = sd_read_next_event(&nextEvent);
    }

}