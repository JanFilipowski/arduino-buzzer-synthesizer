#include "logger.h"

// File handle for the log file ("player.log")
static File    logFile;

// Current write index (0..LOG_MAX_ENTRIES-1) into the circular log
static uint16_t logIndex = 0;

// Temporary buffer for building a single log record
static char recordBuf[LOG_RECORD_SIZE];

/**
 * @brief Initialize the logging system.
 * 
 * - Calls SD.begin() on the given chip-select pin.
 * - If “player.log” does not exist, creates it and pre-allocates
 *   LOG_MAX_ENTRIES * LOG_RECORD_SIZE bytes filled with spaces and newlines.
 * - Opens “player.log” in read/write mode (without append).
 * - Resets the circular index to zero.
 * 
 * @param csPin  SD card chip-select pin.
 * @return true on successful initialization; false on any error.
 */
bool log_init(uint8_t csPin) {
    // Initialize SD interface
    if (!SD.begin(csPin)) return false;

    // Pre-allocate log file if it doesn't exist
    if (!SD.exists("player.log")) {
        File f = SD.open("player.log", FILE_WRITE);
        if (!f) return false;

        // Fill buffer with spaces, ending with '\n'
        memset(recordBuf, ' ', LOG_RECORD_SIZE - 1);
        recordBuf[LOG_RECORD_SIZE - 1] = '\n';

        // Write out LOG_MAX_ENTRIES blank records
        for (int i = 0; i < LOG_MAX_ENTRIES; i++) {
            f.write((uint8_t*)recordBuf, LOG_RECORD_SIZE);
        }
        f.close();
    }

    // Open the log file for read/write (no append)
    logFile = SD.open("player.log", O_RDWR);
    if (!logFile) return false;

    logIndex = 0;  // start at the beginning of the circular buffer
    return true;
}

/**
 * @brief Record an event message into the circular log.
 * 
 * - Builds a fixed-width record: "[timestamp ms] [msg][padding]...\n"
 * - Seeks to the correct offset based on logIndex.
 * - Overwrites the existing record in-place.
 * - Flushes to ensure data is written to the card.
 * - Increments logIndex, wrapping around at LOG_MAX_ENTRIES.
 * 
 * @param msg  Null-terminated short event description.
 */
void log_event(const char* msg) {
    if (!logFile) return;  // no log file available

    // 1) Build the record: zero-padded 10-digit timestamp + space + msg
    unsigned long t = millis();
    int n = snprintf(recordBuf, LOG_RECORD_SIZE,
                     "%010lu %s", t, msg);
    if (n < 0) return;

    // 2) Pad the remainder of the record with spaces, leave last byte for '\n'
    if (n < LOG_RECORD_SIZE - 1) {
        memset(recordBuf + n, ' ', (LOG_RECORD_SIZE - 1) - n);
    }
    recordBuf[LOG_RECORD_SIZE - 1] = '\n';

    // 3) Compute byte offset in file and write the record there
    uint32_t offset = (uint32_t)logIndex * LOG_RECORD_SIZE;
    logFile.seek(offset);
    logFile.write((uint8_t*)recordBuf, LOG_RECORD_SIZE);
    logFile.flush();  // ensure immediate write to SD

    // 4) Advance circular index
    logIndex = (logIndex + 1) % LOG_MAX_ENTRIES;
}
