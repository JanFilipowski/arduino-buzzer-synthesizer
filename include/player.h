#ifndef PLAYER_H
#define PLAYER_H

#include <Arduino.h>
#include <Tone.h>
#include "sd_card.h"
#include <math.h>

#define MAX_ACTIVE_EVENTS 10

// Jeżeli buzzerPins ma 2 elementy, NUM_BUZZERS musi być 2!
#define NUM_BUZZERS       5 

void player_init(void);
void player_update(unsigned long currentTime);
bool player_is_idle(void);
void player_stop_all(void);    // prototyp stopujący wszystkie dźwięki
void player_modify_transpose(int semitones);
void player_seek(unsigned long newTime, const char* filename);

#endif // PLAYER_H
