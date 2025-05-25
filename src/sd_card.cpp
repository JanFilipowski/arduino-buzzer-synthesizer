// sd_card.cpp
// Implements SD card operations for listing CSV files and reading NoteEvent records.

#include "sd_card.h"

// --- Static module state ---
// File handle for the currently opened CSV file
static File   noteFile;
// Flag indicating whether we've reached end of file or encountered an error
static bool   finished;

// Storage for directory listing of CSV filenames
static char   fileNames[MAX_FILES][MAX_FN_LEN];
static uint8_t fileCount;

// ----------------------------------------------------------------------------
// sd_init(csPin)
//   Initialize the SD card using the given chip-select pin.
//   Returns true if the card is successfully initialized, false otherwise.
// ----------------------------------------------------------------------------
bool sd_init(uint8_t csPin) {
    return SD.begin(csPin);
}

// ----------------------------------------------------------------------------
// sd_list_csv_files()
//   Scan the root directory for files ending in “.csv” (case-insensitive).
//   Store up to MAX_FILES names in the fileNames array.
// ----------------------------------------------------------------------------
void sd_list_csv_files(void) {
    fileCount = 0;

    // Open root directory
    File root = SD.open("/");
    if (!root) return;

    root.rewindDirectory();
    File entry;

    // Iterate through each directory entry
    while ((entry = root.openNextFile())) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            size_t len = strlen(name);

            // Check for “.csv” extension
            if (len > 4 && strcasecmp(name + len - 4, ".csv") == 0) {
                if (fileCount < MAX_FILES) {
                    // Copy filename into our array (ensuring null termination)
                    strncpy(fileNames[fileCount], name, MAX_FN_LEN);
                    fileNames[fileCount][MAX_FN_LEN - 1] = '\0';
                    fileCount++;
                }
            }
        }
        entry.close();
    }

    root.close();
}

// ----------------------------------------------------------------------------
// sd_get_file_count()
//   Return the number of CSV files found by the last directory scan.
// ----------------------------------------------------------------------------
uint8_t sd_get_file_count(void) {
    return fileCount;
}

// ----------------------------------------------------------------------------
// sd_get_file_name(idx)
//   Return the filename at index idx (0 <= idx < fileCount).
//   Returns nullptr if idx is out of range.
// ----------------------------------------------------------------------------
const char* sd_get_file_name(uint8_t idx) {
    if (idx < fileCount) {
        return fileNames[idx];
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// sd_open_file(filename)
//   Close any previously open file and open the specified CSV.
//   Skip the header row and reset the finished flag.
//   Returns true on success, false on failure.
// ----------------------------------------------------------------------------
bool sd_open_file(const char* filename) {
    // Close previous file if open
    if (noteFile) {
        noteFile.close();
    }

    // Open new file
    noteFile = SD.open(filename);
    if (!noteFile) {
        finished = true;
        return false;
    }

    // Skip header line (column names)
    sd_skip_header();
    finished = false;
    return true;
}

// ----------------------------------------------------------------------------
// sd_skip_header()
//   Read and discard characters up to the first newline.
//   Used to skip the CSV header row.
// ----------------------------------------------------------------------------
void sd_skip_header(void) {
    noteFile.readStringUntil('\n');
}

// ----------------------------------------------------------------------------
// sd_read_next_event(event)
//   Read the next non-empty CSV line and parse it into a NoteEvent.
//   Returns true if an event was successfully read, false if EOF or error.
// ----------------------------------------------------------------------------
bool sd_read_next_event(NoteEvent* event) {
    // If already finished or no data, signal EOF
    if (finished || !noteFile.available()) {
        finished = true;
        return false;
    }

    // Read next line up to newline
    String line = noteFile.readStringUntil('\n');
    line.trim();

    // Skip empty lines recursively
    if (line.length() == 0) {
        return sd_read_next_event(event);
    }

    // Find commas separating columns: frequency,start,end,buzzer
    int i1 = line.indexOf(',');
    int i2 = line.indexOf(',', i1 + 1);
    int i3 = line.indexOf(',', i2 + 1);
    int i4 = line.indexOf(',', i3 + 1);

    // If format is invalid, log error and skip line
    if (i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0) {
        Serial.print("CSV parse error: "); 
        Serial.println(line);
        return sd_read_next_event(event);
    }

    // Parse each field into the NoteEvent struct
    event->frequency = line.substring(i1 + 1, i2).toInt();
    event->startTime = line.substring(i2 + 1, i3).toInt();
    event->endTime   = line.substring(i3 + 1, i4).toInt();
    event->buzzer    = line.substring(i4 + 1).toInt();

    return true;
}

// ----------------------------------------------------------------------------
// sd_finished()
//   Return true if all events have been read or an error occurred.
// ----------------------------------------------------------------------------
bool sd_finished(void) {
    return finished;
}
