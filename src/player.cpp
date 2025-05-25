#include "player.h"
#include "logger.h"  

// --- Static state for note scheduling ---
// Next upcoming note event from CSV, and flag indicating if it's loaded
static NoteEvent nextEvent;
static bool      nextLoaded;

// Array of currently active (playing) note events and its count
static NoteEvent activeEvents[MAX_ACTIVE_EVENTS];
static uint8_t   activeCount;

// Buzzer hardware: pin assignments and Tone objects
static const int buzzerPins[NUM_BUZZERS] = {28, 29, 30, 31, 32};
static Tone      buzzers[NUM_BUZZERS];

// Initialization flag: configure buzzers only once
static bool initiated = false;

// Playback control factors
static double tempoFactor        = 1.0;  // Speed multiplier (1.0 = normal speed)
static double transposeFactor    = 1.0;  // Frequency multiplier for transpose
static int    transposeSemitones = 0;    // Total semitone offset applied

// Name of the currently open CSV file (used for seeking)
static const char* currentFile = nullptr;

// -----------------------------------------------------------------------------
// player_init()
//   - Set up each buzzer pin via Tone.begin() (only once).
//   - Reset tempo, transpose, and active event list.
//   - Preload the first NoteEvent from the open CSV.
// -----------------------------------------------------------------------------
void player_init(void) {
    if (!initiated) {
        for (uint8_t i = 0; i < NUM_BUZZERS; i++) {
            buzzers[i].begin(buzzerPins[i]);
        }
        initiated = true;
    }
    tempoFactor     = 1.0;
    transposeFactor = 1.0;
    activeCount     = 0;
    nextLoaded      = sd_read_next_event(&nextEvent);
    currentFile     = nullptr;
}

// -----------------------------------------------------------------------------
// player_stop_all()
//   - Immediately stop all currently playing notes.
//   - Clear the activeEvents queue.
// -----------------------------------------------------------------------------
void player_stop_all(void) {
    for (uint8_t i = 0; i < activeCount; i++) {
        int idx = activeEvents[i].buzzer - 1;
        if (idx >= 0 && idx < NUM_BUZZERS) {
            buzzers[idx].stop();
        }
    }
    activeCount = 0;
    interrupts(); // Ensure interrupts are enabled for Tone timing
}

// -----------------------------------------------------------------------------
// player_modify_transpose(semitones)
//   - Adjust global semitone offset.
//   - Recompute transposeFactor = 2^(semitones/12).
// -----------------------------------------------------------------------------
void player_modify_transpose(int semitones) {
    transposeSemitones += semitones;
    transposeFactor = pow(2.0, transposeSemitones / 12.0);
}

// -----------------------------------------------------------------------------
// player_update(currentTime)
//   - Called in loop() when playing.
//   - Starts any notes whose startTime ≤ currentTime*tempoFactor.
//   - Stops notes whose endTime ≤ currentTime*tempoFactor.
// -----------------------------------------------------------------------------
void player_update(unsigned long currentTime) {
    interrupts(); // Allow Tone library interrupts for accurate timing

    // Start new notes as long as their scheduled time has arrived
    while (nextLoaded && nextEvent.startTime <= currentTime * tempoFactor) {
        int idx = nextEvent.buzzer - 1;
        if (idx >= 0 && idx < NUM_BUZZERS) {
            uint16_t freq = (uint16_t)(nextEvent.frequency * transposeFactor);
            buzzers[idx].play(freq);
            if (activeCount < MAX_ACTIVE_EVENTS) {
                activeEvents[activeCount++] = nextEvent;
            }
        }
        nextLoaded = sd_read_next_event(&nextEvent);
    }

    // Stop any notes whose end time has passed
    for (uint8_t i = 0; i < activeCount; ) {
        if (activeEvents[i].endTime <= currentTime * tempoFactor) {
            int idx = activeEvents[i].buzzer - 1;
            if (idx >= 0 && idx < NUM_BUZZERS) {
                buzzers[idx].stop();
            }
            // Remove this event by shifting the array down
            for (uint8_t j = i; j + 1 < activeCount; j++) {
                activeEvents[j] = activeEvents[j + 1];
            }
            activeCount--;
        } else {
            i++;
        }
    }
}

// -----------------------------------------------------------------------------
// player_is_idle()
//   - Returns true if no more events are loaded and no notes are sounding.
// -----------------------------------------------------------------------------
bool player_is_idle(void) {
    return (!nextLoaded && activeCount == 0);
}

// -----------------------------------------------------------------------------
// player_seek(newTime, filename)
//   - Perform a “seek” to newTime within the current file.
//   - Stops all notes, reopens the file, and parses events up to newTime.
//   - Any notes still sounding at newTime are started immediately.
// -----------------------------------------------------------------------------
void player_seek(unsigned long newTime, const char* filename) {
    // 1) Stop all currently playing notes
    player_stop_all();

    // 2) Reopen the CSV file and reset state
    currentFile = filename;
    sd_open_file(currentFile);
    activeCount = 0;
    nextLoaded  = sd_read_next_event(&nextEvent);

    // 3) Parse events until newTime
    while (nextLoaded && nextEvent.startTime <= newTime) {
        // If a note overlaps newTime, start it now
        if (nextEvent.endTime > newTime) {
            int idx = nextEvent.buzzer - 1;
            if (idx >= 0 && idx < NUM_BUZZERS) {
                uint16_t freq = (uint16_t)(nextEvent.frequency * transposeFactor);
                buzzers[idx].play(freq);
                if (activeCount < MAX_ACTIVE_EVENTS) {
                    activeEvents[activeCount++] = nextEvent;
                }
            }
        }
        nextLoaded = sd_read_next_event(&nextEvent);
    }
}
