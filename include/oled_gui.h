#ifndef OLED_GUI_H
#define OLED_GUI_H

#include <Arduino.h>

// wywołać raz w setup()
void oled_init();

// pokaż przewijaną listę plików, podświetl wybrany
void oled_show_file_list(const char* list[], uint8_t count, uint8_t selected);

// pokaż ekran pauzy
void oled_show_paused();

void oled_show_loading();
void oled_show_error(const char* msg);
void oled_show_playback_menu(const char* opts[], uint8_t count, uint8_t sel, const char* filename, unsigned long playertime, unsigned long status, double tempo, long transpose);

#endif // OLED_GUI_H
