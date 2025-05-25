#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <SD.h>

#define MAX_FILES    12
#define MAX_FN_LEN   32

// -----------------------------------------------------------------------------
//   Struktura zdarzenia nutowego – jak dotychczas
// -----------------------------------------------------------------------------
struct NoteEvent {
    uint16_t      frequency;
    unsigned long startTime;
    unsigned long endTime;
    uint8_t       buzzer;
};

// -----------------------------------------------------------------------------
//   Podstawowe operacje na CSV
// -----------------------------------------------------------------------------
bool     sd_init(uint8_t csPin);
bool     sd_open_file(const char* filename);
void     sd_skip_header(void);
bool     sd_read_next_event(NoteEvent* e);
bool     sd_finished(void);

// -----------------------------------------------------------------------------
//   Nowe: przeglądanie katalogu SD
// -----------------------------------------------------------------------------
void     sd_list_csv_files(void);
/** @return liczbę znalezionych plików .csv */
uint8_t  sd_get_file_count(void);
/** @return wskaźnik na nazwę pliku o indeksie [0..count-1] */
const char* sd_get_file_name(uint8_t idx);

#endif // SD_CARD_H
