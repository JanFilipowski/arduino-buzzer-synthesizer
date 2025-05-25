#ifndef OLED_GUI_H
#define OLED_GUI_H

#include <Arduino.h>

/**
 * @brief Initialize the OLED/TFT display.
 *
 * Must be called once in setup() before any other display functions.
 */
void oled_init();

/**
 * @brief Display a scrollable list of filenames, highlighting the selected entry.
 *
 * @param list      Array of null-terminated strings with filenames.
 * @param count     Number of entries in the list.
 * @param selected  Index of the currently highlighted filename.
 */
void oled_show_file_list(const char* list[], uint8_t count, uint8_t selected);

/**
 * @brief Display a "PAUSED" screen.
 *
 * Clears the display and shows the PAUSED message centered.
 */
void oled_show_paused();

/**
 * @brief Display a "Loading..." screen.
 *
 * Clears the display and shows a loading message centered.
 */
void oled_show_loading();

/**
 * @brief Display an error message on the screen.
 *
 * @param msg  Null-terminated string with the error description.
 */
void oled_show_error(const char* msg);

/**
 * @brief Show the playback control menu with current status info.
 *
 * Renders a vertical list of playback option icons, highlights the selected one,
 * and displays filename, elapsed time, play/pause status, tempo, and transpose.
 *
 * @param opts       Array of null-terminated strings for playback option icons.
 * @param count      Number of options in opts[].
 * @param sel        Index of the currently selected option.
 * @param filename   Name of the file being played (without ".csv").
 * @param playertime Current playback time in milliseconds.
 * @param status     Playback status flag (0 = playing, 1 = paused).
 * @param tempo      Current playback speed multiplier.
 * @param transpose  Current transpose offset in semitones.
 */
void oled_show_playback_menu(const char* opts[],
                             uint8_t count,
                             uint8_t sel,
                             const char* filename,
                             unsigned long playertime,
                             unsigned long status,
                             double tempo,
                             long transpose);

#endif // OLED_GUI_H
