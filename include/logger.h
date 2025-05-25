#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <SD.h>

#define LOG_MAX_ENTRIES 1000
#define LOG_RECORD_SIZE 64    // bajtów na wpis włącznie z '\n'

/**
 * @brief Inicjalizuje system logowania: tworzy lub otwiera player.log i
 *        rozszerza go do LOG_MAX_ENTRIES*LOG_RECORD_SIZE bajtów,
 *        jeżeli to nowy plik.
 */
bool   log_init(uint8_t csPin);

/**
 * @brief Zapisuje wiadomość do pliku logowego (cyklicznie nadpisując stare wpisy).
 * @param msg Krótki tekst (max ~LOG_RECORD_SIZE-20 bajtów), np. "PLAY start track.csv"
 */
void   log_event(const char* msg);

#endif // LOGGER_H
