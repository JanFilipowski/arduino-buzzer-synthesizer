#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "sd_card.h"
#include "player.h"
#include "oled_gui.h"
#include "logger.h"

#define CHIP_SELECT_PIN    53
#define BTN_UP_PIN         22   // fizycznie: dół ekranu
#define BTN_OK_PIN         23   // środek
#define BTN_DOWN_PIN       24   // fizycznie: góra ekranu
#define MAX_FILES_COUNT    MAX_FILES
#define SEEK_BUFFER_DELAY  1000UL

enum AppState {
  STATE_MENU,
  STATE_PLAYING,
  STATE_PAUSED
};

static AppState state;
static uint8_t  selIndex;
static uint8_t  fileCount;
static const char* fileList[MAX_FILES_COUNT];

// odtwarzanie
static double        playTime            = 0.0;
static unsigned long lastMillis          = 0;
static double        tempoFactor         = 1.0;
static long          pendingSeekDeltaMs  = 0;
static unsigned long lastSeekRequestMs   = 0;
static unsigned long timeSinceLastRefresh = 0;
static long transposeValue = 0;

// menu playback-owe
static char* playbackOpts[] = {
  "||",  // Play/Pause
  "/D",     // Stop
  ">>",    // Forward
  "<<",    // Rewind
  "S+",    // Speed +
  "S-",     // Speed –
  "T+",    // Transpose +
  "T-"    // Transpose –
};
static const uint8_t playbackCount = sizeof(playbackOpts) / sizeof(playbackOpts[0]);
static uint8_t       playSel       = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait */ }

  // ekran startowy
  oled_init();
  oled_show_loading();

  // przyciski
  pinMode(BTN_UP_PIN,   INPUT_PULLUP);
  pinMode(BTN_OK_PIN,   INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  // SD + lista plików
  if (!sd_init(CHIP_SELECT_PIN)) {
    oled_show_error("SD init error");
    while (1) delay(100);
  }
  if (!log_init(CHIP_SELECT_PIN)) {
    oled_show_error("Log init error");
  }

  sd_list_csv_files();
  fileCount = sd_get_file_count();
  for (uint8_t i = 0; i < fileCount; i++) {
    fileList[i] = sd_get_file_name(i);
  }

  // start w menu plików
  state     = STATE_MENU;
  selIndex  = 0;
  playSel   = 0;
  tempoFactor        = 1.0;
  playTime           = 0.0;
  pendingSeekDeltaMs = 0;
  lastSeekRequestMs  = 0;

  oled_show_file_list(fileList, fileCount, selIndex);
  log_event("APP START");

  Serial.println(F("\n=== Komendy Serial ==="));
  Serial.println(F("z = rewind 5s, x = forward 5s"));
  Serial.println(F("w/q = tempo +/-, [/] = transpose -/+"));
  Serial.println(F("p = PLAY/PAUSE, s = STOP"));
}

void loop() {
  interrupts();

  // 1) update czasu odtwarzania
  if (state == STATE_PLAYING) {
    unsigned long now = millis();
    playTime   += (now - lastMillis) * tempoFactor;
    timeSinceLastRefresh += now - lastMillis;
    lastMillis = now;
  }

  // 2) MENU PLIKÓW
  if (state == STATE_MENU) {
    transposeValue = 0;

    static bool lastUp = HIGH, lastOk = HIGH, lastDown = HIGH;
    bool curUp   = digitalRead(BTN_UP_PIN);
    bool curOk   = digitalRead(BTN_OK_PIN);
    bool curDown = digitalRead(BTN_DOWN_PIN);

    // fizyczny dół → przewiń w dół po liście (selIndex++)
    if (lastUp == HIGH && curUp == LOW) {
      if (selIndex + 1 < fileCount) {
        selIndex++;
        oled_show_file_list(fileList, fileCount, selIndex);
        log_event("Menu DOWN");
      }
    }
    // fizyczny góra → przewiń w górę (selIndex--)
    if (lastDown == HIGH && curDown == LOW) {
      if (selIndex > 0) {
        selIndex--;
        oled_show_file_list(fileList, fileCount, selIndex);
        log_event("Menu UP");
      }
    }
    // środek → OK
    if (lastOk == HIGH && curOk == LOW) {
      const char* fn = fileList[selIndex];
      log_event(strcat("Playing -> ", fn));
      if (sd_open_file(fn)) {
        player_init();
        playTime           = 0.0;   
        lastMillis         = millis();
        state              = STATE_PLAYING;
        pendingSeekDeltaMs = 0;
        playSel            = 0;
        oled_show_playback_menu(playbackOpts, playbackCount, playSel, fn, (unsigned long)playTime, 0, tempoFactor, transposeValue);
        timeSinceLastRefresh = 0;
        log_event("Playback START");
      } else {
        oled_show_error("Open failed");
        log_event("Playback FAIL");
      }
    }

    lastUp   = curUp;
    lastOk   = curOk;
    lastDown = curDown;
    return;
  }

  // 3) MENU PLAYBACK-OWE (PLAYING i PAUSED)
  if (state == STATE_PLAYING || state == STATE_PAUSED) {
    static bool lastUp = HIGH, lastOk = HIGH, lastDown = HIGH;
    bool curUp   = digitalRead(BTN_UP_PIN);
    bool curOk   = digitalRead(BTN_OK_PIN);
    bool curDown = digitalRead(BTN_DOWN_PIN);

    // przewiń opcje w górę
    if (lastDown == HIGH && curDown == LOW) {
      playSel = (playSel == 0) ? playbackCount - 1 : playSel - 1;
      oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
      timeSinceLastRefresh = 0;
    }
    // przewiń opcje w dół
    if (lastUp == HIGH && curUp == LOW) {
      playSel = (playSel + 1) % playbackCount;
      oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
      timeSinceLastRefresh = 0;
    }
    // OK → wykonaj akcję
    if (lastOk == HIGH && curOk == LOW && playTime > 0.5) {
      switch (playSel) {
        case 0:  // Play/Pause
          if (state == STATE_PLAYING) {
            player_stop_all();
            state = STATE_PAUSED;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
            log_event("Paused");
          } else {
            lastMillis = millis();
            state      = STATE_PLAYING;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
            timeSinceLastRefresh  = 0;
            log_event("Resumed");
          }
          break;
        case 1:  // Stop
          player_stop_all();
          state = STATE_MENU;
          oled_show_file_list(fileList, fileCount, selIndex);
          log_event("Stopped");
          break;
        case 6:  // Transpose +
          player_modify_transpose(+1);
          transposeValue++;
          log_event("Transpose +1");
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
        case 7:  // Transpose –
          player_modify_transpose(-1);
          transposeValue--;
          log_event("Transpose -1");
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
        case 2:  // Forward
            playTime   += 5000.0;
            lastMillis  = millis();
            player_stop_all();
            Serial.println(F("[CMD] Forward 5s"));
            log_event("Forward 5s");
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
        case 3:  // Rewind
          pendingSeekDeltaMs -= 5000;
          if ((unsigned long)playTime - 5000 < 0) pendingSeekDeltaMs = -(unsigned long)playTime;
          lastSeekRequestMs  = millis();
          log_event("Rewind 5s");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], 
            ((unsigned long)playTime + pendingSeekDeltaMs > 0 ? (unsigned long)playTime + pendingSeekDeltaMs : 0), 
            (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
        case 4:  // Speed +
          tempoFactor += 0.1;
          log_event("Speed +0.1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
        case 5:  // Speed –
          tempoFactor = max(0.1, tempoFactor - 0.1);
          log_event("Speed -0.1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
          break;
      }
    }

    lastUp   = curUp;
    lastOk   = curOk;
    lastDown = curDown;
  }

  // 4) SERIALOWE KOMENDY (PLAYING / PAUSED i globalne)
  
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd!='\n' && cmd!='\r') {
      switch (state) {
        case STATE_PLAYING:
          if (cmd=='p') {
            player_stop_all();
            state = STATE_PAUSED;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
            log_event("Paused");
          } else if (cmd=='s') {
            player_stop_all();
            state = STATE_MENU;
            oled_show_file_list(fileList, fileCount, selIndex);
            log_event("Stopped");
          }
          break;
        case STATE_PAUSED:
          if (cmd=='p') {
            lastMillis = millis();
            state      = STATE_PLAYING;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
            timeSinceLastRefresh = 0;
            log_event("Resumed");
          } else if (cmd=='s') {
            player_stop_all();
            state = STATE_MENU;
            oled_show_file_list(fileList, fileCount, selIndex);
            log_event("Stopped");
          }
          break;
        default: break;
      }
      // rewind/forward
      if (cmd=='z') {
        pendingSeekDeltaMs -= 5000;
        lastSeekRequestMs = millis();
        Serial.println(F("[CMD] Buffered rewind 5s"));
        log_event("Rewind 5s");
      }
      if (cmd=='x') {
        playTime   += 5000.0;
        lastMillis  = millis();
        player_stop_all();
        Serial.println(F("[CMD] Forward 5s"));
        log_event("Forward 5s");
      }
      // tempo
      if (cmd=='w') {
        tempoFactor += 0.1;
        Serial.print(F("[CMD] Tempo+ → ")); Serial.println(tempoFactor);
        log_event("Tempo +0.1");
      }
      if (cmd=='q') {
        tempoFactor -= 0.1;
        if (tempoFactor<0.1) tempoFactor=0.1;
        Serial.print(F("[CMD] Tempo- → ")); Serial.println(tempoFactor);
        log_event("Tempo -0.1");
      }
      // transpose
      if (cmd==']') {
        transposeValue++;
        player_modify_transpose(+1);
        Serial.println(F("[CMD] Transpose+"));
        log_event("Transpose +1");
      }
      if (cmd=='[') {
        transposeValue--;
        player_modify_transpose(-1);
        Serial.println(F("[CMD] Transpose-"));
        log_event("Transpose -1");
      }
    }
  }

  // 5) buforowany seek
  if ((state == STATE_PLAYING || state == STATE_PAUSED)
      && pendingSeekDeltaMs != 0
      && (millis() - lastSeekRequestMs) >= SEEK_BUFFER_DELAY) {
    double nt = playTime + pendingSeekDeltaMs;
    if (nt < 0.0) nt = 0.0;
    player_seek((unsigned long)nt, fileList[selIndex]);
    playTime           = nt;
    lastMillis         = millis();
    pendingSeekDeltaMs = 0;
    log_event("Executed seek");
    oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, (state == STATE_PLAYING ? 0 : 1), tempoFactor, transposeValue);
  }

  // 6) update player
  if (state == STATE_PLAYING) {
    player_update((unsigned long)playTime);

    if (timeSinceLastRefresh > 9000){
            oled_show_playback_menu(playbackOpts, playbackCount, playSel, fileList[selIndex], (unsigned long)playTime, 0, tempoFactor, transposeValue); timeSinceLastRefresh = 0;
    }

    if (sd_finished() && player_is_idle()) {
      state = STATE_MENU;
      oled_show_file_list(fileList, fileCount, selIndex);
      log_event("End of song");
    }
  }
}
