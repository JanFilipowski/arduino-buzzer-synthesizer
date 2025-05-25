#ifndef PLAYER_H
#define PLAYER_H

#include <Arduino.h>
#include <Tone.h>
#include "sd_card.h"    // Provides NoteEvent struct definition

// Maximum number of simultaneous active note events
#define MAX_ACTIVE_EVENTS 10

// Number of buzzers (pins) used for polyphonic playback.
// Must match the length of the buzzerPins array in player.cpp.
#define NUM_BUZZERS       5

/**
 * @brief Initialize the playback engine.
 *
 * Sets up Tone objects on each buzzer pin (only once),
 * resets tempo and transpose factors, clears active events,
 * and preloads the first NoteEvent from the open CSV file.
 */
void player_init(void);

/**
 * @brief Drive the playback engine.
 *
 * Should be called repeatedly (e.g., in loop()) with the
 * current playback time (in milliseconds). Starts any
 * new notes whose startTime ≤ currentTime*tempoFactor,
 * and stops notes whose endTime ≤ currentTime*tempoFactor.
 *
 * @param currentTime  Playback time in ms, multiplied by tempoFactor.
 */
void player_update(unsigned long currentTime);

/**
 * @brief Query whether playback has completed.
 *
 * @return true if no future events are loaded and no
 *         active notes remain sounding.
 */
bool player_is_idle(void);

/**
 * @brief Immediately stop all currently playing notes.
 *
 * Stops each active buzzer and clears the active events list.
 * Also re-enables interrupts to allow normal operation.
 */
void player_stop_all(void);

/**
 * @brief Adjust the global transpose setting.
 *
 * Modifies the pitch of all subsequently played notes by the
 * given number of semitones. Internally updates a frequency
 * scaling factor = 2^(semitones/12).
 *
 * @param semitones  Number of semitones to shift (positive or negative).
 */
void player_modify_transpose(int semitones);

/**
 * @brief Seek playback to a new time within the current file.
 *
 * Stops all buzzers, reopens the current file, and parses events
 * up to newTime. Any notes that would still be sounding at newTime
 * are started immediately.
 *
 * @param newTime   Target playback time in ms.
 * @param filename  Name of the CSV file currently open on SD.
 */
void player_seek(unsigned long newTime, const char* filename);

#endif // PLAYER_H
