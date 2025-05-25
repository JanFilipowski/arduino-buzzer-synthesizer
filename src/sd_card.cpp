#include "sd_card.h"

static File   noteFile;
static bool   finished;

// dla listy plików:
static char   fileNames[MAX_FILES][MAX_FN_LEN];
static uint8_t fileCount;

bool sd_init(uint8_t csPin) {
    return SD.begin(csPin);
}

void sd_list_csv_files(void) {
    fileCount = 0;
    File root = SD.open("/");
    if (!root) return;
    root.rewindDirectory();
    File entry;
    while ((entry = root.openNextFile())) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            size_t len = strlen(name);
            if (len > 4 && strcasecmp(name + len - 4, ".csv") == 0) {
                if (fileCount < MAX_FILES) {
                    strncpy(fileNames[fileCount], name, MAX_FN_LEN);
                    fileNames[fileCount][MAX_FN_LEN-1] = '\0';
                    fileCount++;
                }
            }
        }
        entry.close();
    }
    root.close();
}

uint8_t sd_get_file_count(void) {
    return fileCount;
}

const char* sd_get_file_name(uint8_t idx) {
    if (idx < fileCount) return fileNames[idx];
    return NULL;
}

bool sd_open_file(const char* filename) {
    if (noteFile) noteFile.close();
    noteFile = SD.open(filename);
    if (!noteFile) {
        finished = true;
        return false;
    }
    sd_skip_header();
    finished = false;
    return true;
}

void sd_skip_header(void) {
    noteFile.readStringUntil('\n');
}

bool sd_read_next_event(NoteEvent* event) {
    if (finished || !noteFile.available()) {
        finished = true;
        return false;
    }
    String line = noteFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
        return sd_read_next_event(event);
    }
    int i1 = line.indexOf(',');
    int i2 = line.indexOf(',', i1 + 1);
    int i3 = line.indexOf(',', i2 + 1);
    int i4 = line.indexOf(',', i3 + 1);
    if (i1<0||i2<0||i3<0||i4<0) {
        Serial.print("Błąd CSV: "); Serial.println(line);
        return sd_read_next_event(event);
    }
    event->frequency = line.substring(i1+1, i2).toInt();
    event->startTime = line.substring(i2+1, i3).toInt();
    event->endTime   = line.substring(i3+1, i4).toInt();
    event->buzzer    = line.substring(i4+1).toInt();
    return true;
}

bool sd_finished(void) {
    return finished;
}
