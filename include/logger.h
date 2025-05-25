#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <SD.h>

// Maximum number of log entries to retain (older entries are overwritten circularly)
#define LOG_MAX_ENTRIES 1000

// Fixed byte size per record in the log file (including newline '\n')
#define LOG_RECORD_SIZE 64

/**
 * @brief Initialize the logging system.
 *
 * Opens (or creates) "player.log" on the SD card using the given chip-select pin.
 * If the file is newly created, pre-allocates it to LOG_MAX_ENTRIES * LOG_RECORD_SIZE bytes
 * so that entries can be written in-place in a circular buffer fashion.
 *
 * @param csPin  SD card chip-select pin.
 * @return true if initialization (SD.begin and file setup) succeeded; false otherwise.
 */
bool log_init(uint8_t csPin);

/**
 * @brief Write an event message to the log file.
 *
 * Appends (or overwrites, in circular fashion) a single log entry containing:
 *   [timestamp] msg\n
 * ensuring each record occupies exactly LOG_RECORD_SIZE bytes.
 * When reaching LOG_MAX_ENTRIES, wraps around to the beginning of the file.
 *
 * @param msg  Null-terminated event description (up to ~LOG_RECORD_SIZE-20 bytes recommended).
 */
void log_event(const char* msg);

#endif // LOGGER_H
