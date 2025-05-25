// sd_card.h
// SD card interface for listing and reading note-event CSV files.

#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <SD.h>

// Maximum number of CSV files to index on SD card
#define MAX_FILES    12
// Maximum length of a filename (including null terminator)
#define MAX_FN_LEN   32

/**
 * @struct NoteEvent
 * @brief Represents a single musical note event loaded from a CSV file.
 *
 * @var frequency   Frequency of the note in Hz
 * @var startTime   Time (ms) when the note should start
 * @var endTime     Time (ms) when the note should end
 * @var buzzer      1-based index of the buzzer to play this note
 */
struct NoteEvent {
    uint16_t      frequency;
    unsigned long startTime;
    unsigned long endTime;
    uint8_t       buzzer;
};

/// @name CSV File I/O Operations
/// @{

/**
 * @brief Initialize the SD card interface.
 * @param csPin  Chip-select pin for the SD module.
 * @return true if SD.begin(csPin) succeeds, false otherwise.
 */
bool sd_init(uint8_t csPin);

/**
 * @brief Open a CSV file on the SD card for reading note events.
 * @param filename  Name or path of the CSV file.
 * @return true on successful open, false on failure.
 */
bool sd_open_file(const char* filename);

/**
 * @brief Skip the header row of the currently opened CSV file.
 *        Assumes the first line contains column names.
 */
void sd_skip_header(void);

/**
 * @brief Read the next NoteEvent from the open CSV file.
 * @param e  Pointer to a NoteEvent struct to populate.
 * @return true if an event was read, false if end of file or error.
 */
bool sd_read_next_event(NoteEvent* e);

/**
 * @brief Check whether all events have been read from the current file.
 * @return true if no more events remain, false otherwise.
 */
bool sd_finished(void);

/// @}

/// @name Directory Listing Operations
/// @{

/**
 * @brief Scan the root directory of the SD card for .csv files.
 *        Populates an internal list of filenames.
 */
void sd_list_csv_files(void);

/**
 * @brief Get the number of .csv files found in the last scan.
 * @return Count of CSV files (0..MAX_FILES).
 */
uint8_t sd_get_file_count(void);

/**
 * @brief Retrieve the filename at the given index from the last scan.
 * @param idx  Index in range [0..sd_get_file_count()-1].
 * @return Pointer to a null-terminated filename, or NULL if idx is out of range.
 */
const char* sd_get_file_name(uint8_t idx);

/// @}

#endif // SD_CARD_H
