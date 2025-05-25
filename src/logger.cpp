#include "logger.h"

static File    logFile;
static uint16_t logIndex = 0;

// Pomocniczy bufor na jeden rekord
static char recordBuf[LOG_RECORD_SIZE];

bool log_init(uint8_t csPin) {
    if (!SD.begin(csPin)) return false;

    if (!SD.exists("player.log")) {
        File f = SD.open("player.log", FILE_WRITE);
        if (!f) return false;
        memset(recordBuf, ' ', LOG_RECORD_SIZE-1);
        recordBuf[LOG_RECORD_SIZE-1] = '\n';
        for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
            f.write((uint8_t*)recordBuf, LOG_RECORD_SIZE);
        }
        f.close();
    }

    // Otwórz do odczytu i zapisu, BEZ append!
    logFile = SD.open("player.log", O_RDWR);
    if (!logFile) return false;
    logIndex = 0;
    return true;
}


void log_event(const char* msg) {
    if (!logFile) return;
    // Zbuduj rekord: [timestamp ms][space][msg][padding]...\n
    unsigned long t = millis();
    int n = snprintf(recordBuf, LOG_RECORD_SIZE,
                     "%010lu %s", t, msg);
    if (n < 0) return;
    // dopełnij spacjami
    if (n < LOG_RECORD_SIZE-1) {
        memset(recordBuf + n, ' ', LOG_RECORD_SIZE-1 - n);
    }
    recordBuf[LOG_RECORD_SIZE-1] = '\n';

    // Szukajemy pozycji w pliku
    uint32_t offset = (uint32_t)logIndex * LOG_RECORD_SIZE;
    logFile.seek(offset);
    logFile.write((uint8_t*)recordBuf, LOG_RECORD_SIZE);
    logFile.flush();              // wymuś zapis na kartę
    // Aktualizuj indeks
    logIndex = (logIndex + 1) % LOG_MAX_ENTRIES;
}
