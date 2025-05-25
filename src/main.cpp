#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "sd_card.h"    // SD card file listing and CSV parsing
#include "player.h"     // Playback engine for note events
#include "oled_gui.h"   // OLED/TFT display interface
#include "logger.h"     // Event logging to SD card

// Pin assignments
#define CHIP_SELECT_PIN    53    // SD card chip select
#define BTN_UP_PIN         22    // UP button (physically bottom of screen)
#define BTN_OK_PIN         23    // OK button (middle)
#define BTN_DOWN_PIN       24    // DOWN button (physically top of screen)
#define MAX_FILES_COUNT    MAX_FILES
#define SEEK_BUFFER_DELAY  1000UL  // Delay (ms) for buffered seek to avoid frequent file seeks

// Application states
enum AppState {
  STATE_MENU,     // File selection menu
  STATE_PLAYING,  // Playback in progress
  STATE_PAUSED    // Playback paused
};

// Global application variables
static AppState state;                   // Current state of the app
static uint8_t selIndex;                 // Index of selected file in list
static uint8_t fileCount;                // Number of CSV files found on SD
static const char* fileList[MAX_FILES_COUNT];  // Array of file names

// Playback timing and control variables
static double        playTime            = 0.0;    // Current playback time (ms)
static unsigned long lastMillis          = 0;      // Timestamp of last update
static double        tempoFactor         = 1.0;    // Playback speed multiplier
static long          pendingSeekDeltaMs  = 0;      // Buffered seek offset (ms)
static unsigned long lastSeekRequestMs    = 0;     // Timestamp of last seek request
static unsigned long timeSinceLastRefresh = 0;     // Time since last GUI refresh
static long          transposeValue      = 0;      // Pitch shift in semitones

// Playback menu options shown on screen
static char* playbackOpts[] = {
  "||",  // Play/Pause toggle
  "/D",  // Stop playback
  ">>",  // Fast-forward 5s
  "<<",  // Rewind 5s
  "S+",  // Increase speed
  "S-",  // Decrease speed
  "T+",  // Transpose up
  "T-"   // Transpose down
};
static const uint8_t playbackCount = sizeof(playbackOpts) / sizeof(playbackOpts[0]);
static uint8_t playSel = 0;  // Currently highlighted playback option

// -----------------------------------------------------------------------------
// setup()
// Initialize hardware peripherals, load file list, and display initial menu
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial connection */ }

  // Initialize display and show loading screen
  oled_init();
  oled_show_loading();

  // Configure button inputs with internal pull-ups
  pinMode(BTN_UP_PIN,   INPUT_PULLUP);
  pinMode(BTN_OK_PIN,   INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  // Initialize SD card and logger
  if (!sd_init(CHIP_SELECT_PIN)) {
    oled_show_error("SD init error");
    while (1) delay(100);  // Stop execution if SD init fails
  }
  if (!log_init(CHIP_SELECT_PIN)) {
    oled_show_error("Log init error");
  }

  // Enumerate CSV files on SD card
  sd_list_csv_files();
  fileCount = sd_get_file_count();
  for (uint8_t i = 0; i < fileCount; i++) {
    fileList[i] = sd_get_file_name(i);
  }

  // Initialize state variables and display file list
  state               = STATE_MENU;
  selIndex            = 0;
  playSel             = 0;
  tempoFactor         = 1.0;
  playTime            = 0.0;
  pendingSeekDeltaMs  = 0;
  lastSeekRequestMs   = 0;

  oled_show_file_list(fileList, fileCount, selIndex);
  log_event("APP START");

  // Print serial command reference
  Serial.println(F("\n=== Serial Commands ==="));
  Serial.println(F("z = rewind 5s, x = forward 5s"));
  Serial.println(F("w/q = tempo +/-, [/] = transpose -/+"));
  Serial.println(F("p = PLAY/PAUSE, s = STOP"));
}

// -----------------------------------------------------------------------------
// loop()
// Main application loop: update playback time, handle user input, and refresh UI
// -----------------------------------------------------------------------------
void loop() {
  interrupts();  // Ensure timer interrupts for Tone library work correctly

  // ---------------------------------------------------------------------------
  // 1) UPDATER
  //    Update playback time when in PLAYING state
  // ---------------------------------------------------------------------------
  if (state == STATE_PLAYING) {
    unsigned long now = millis();
    playTime            += (now - lastMillis) * tempoFactor;
    timeSinceLastRefresh += now - lastMillis;
    lastMillis = now;
  }

  // ---------------------------------------------------------------------------
  // 2) FILE MENU
  //    Navigate and select CSV files
  // ---------------------------------------------------------------------------
  if (state == STATE_MENU) {
    transposeValue = 0;  // Reset transpose in menu

    static bool lastUp = HIGH, lastOk = HIGH, lastDown = HIGH;
    bool curUp   = digitalRead(BTN_UP_PIN);
    bool curOk   = digitalRead(BTN_OK_PIN);
    bool curDown = digitalRead(BTN_DOWN_PIN);

    // Scroll down (physical bottom of screen)
    if (lastUp == HIGH && curUp == LOW) {
      if (selIndex + 1 < fileCount) {
        selIndex++;
        oled_show_file_list(fileList, fileCount, selIndex);
        log_event("Menu DOWN");
      }
    }
    // Scroll up (physical top of screen)
    if (lastDown == HIGH && curDown == LOW) {
      if (selIndex > 0) {
        selIndex--;
        oled_show_file_list(fileList, fileCount, selIndex);
        log_event("Menu UP");
      }
    }
    // OK button: open selected file and start playback
    if (lastOk == HIGH && curOk == LOW) {
      const char* fn = fileList[selIndex];
      log_event(strcat("Playing -> ", fn));
      if (sd_open_file(fn)) {
        player_init();               // Prepare player state
        playTime           = 0.0;
        lastMillis         = millis();
        state              = STATE_PLAYING;
        pendingSeekDeltaMs = 0;
        playSel            = 0;
        oled_show_playback_menu(playbackOpts, playbackCount, playSel, fn,
                                 (unsigned long)playTime, 0,
                                 tempoFactor, transposeValue);
        timeSinceLastRefresh = 0;
        log_event("Playback START");
      } else {
        oled_show_error("Open failed");
        log_event("Playback FAIL");
      }
    }

    // Update last button states for edge detection
    lastUp   = curUp;
    lastOk   = curOk;
    lastDown = curDown;
    return;  // Skip rest of loop when in menu
  }

  // ---------------------------------------------------------------------------
  // 3) PLAYBACK MENU
  //    Handle Play/Pause, Stop, Seek, Tempo, Transpose
  // ---------------------------------------------------------------------------
  if (state == STATE_PLAYING || state == STATE_PAUSED) {
    static bool lastUp = HIGH, lastOk = HIGH, lastDown = HIGH;
    bool curUp   = digitalRead(BTN_UP_PIN);
    bool curOk   = digitalRead(BTN_OK_PIN);
    bool curDown = digitalRead(BTN_DOWN_PIN);

    // Navigate menu options
    if (lastDown == HIGH && curDown == LOW) {
      playSel = (playSel == 0) ? playbackCount - 1 : playSel - 1;
      oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                               fileList[selIndex], (unsigned long)playTime,
                               (state == STATE_PLAYING ? 0 : 1),
                               tempoFactor, transposeValue);
      timeSinceLastRefresh = 0;
    }
    if (lastUp == HIGH && curUp == LOW) {
      playSel = (playSel + 1) % playbackCount;
      oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                               fileList[selIndex], (unsigned long)playTime,
                               (state == STATE_PLAYING ? 0 : 1),
                               tempoFactor, transposeValue);
      timeSinceLastRefresh = 0;
    }

    // Execute selected action on OK (only if playback has started)
    if (lastOk == HIGH && curOk == LOW && playTime > 0.5) {
      switch (playSel) {
        case 0:  // Play/Pause toggle
          if (state == STATE_PLAYING) {
            player_stop_all();  // Pause by stopping buzzers
            state = STATE_PAUSED;
            log_event("Paused");
          } else {
            lastMillis = millis();
            state      = STATE_PLAYING;
            log_event("Resumed");
          }
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 1:  // Stop playback and return to file menu
          player_stop_all();
          state = STATE_MENU;
          oled_show_file_list(fileList, fileCount, selIndex);
          log_event("Stopped");
          break;

        case 2:  // Fast-forward 5 seconds
          playTime   += 5000.0;
          lastMillis  = millis();
          player_stop_all();
          Serial.println(F("[CMD] Forward 5s"));
          log_event("Forward 5s");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 3:  // Rewind 5 seconds (buffered)
          pendingSeekDeltaMs -= 5000;
          if ((unsigned long)playTime < 5000) pendingSeekDeltaMs = -(unsigned long)playTime;
          lastSeekRequestMs  = millis();
          log_event("Rewind 5s");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex],
                                   (unsigned long)max(0.0, playTime + pendingSeekDeltaMs),
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 4:  // Increase speed
          tempoFactor += 0.1;
          log_event("Speed +0.1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 5:  // Decrease speed
          tempoFactor = max(0.1, tempoFactor - 0.1);
          log_event("Speed -0.1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 6:  // Transpose up one semitone
          transposeValue++;
          player_modify_transpose(+1);
          log_event("Transpose +1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;

        case 7:  // Transpose down one semitone
          transposeValue--;
          player_modify_transpose(-1);
          log_event("Transpose -1");
          oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex], (unsigned long)playTime,
                                   (state == STATE_PLAYING ? 0 : 1),
                                   tempoFactor, transposeValue);
          break;
      }
    }

    // Save button states for next cycle
    lastUp   = curUp;
    lastOk   = curOk;
    lastDown = curDown;
  }
  // ---------------------------------------------------------------------------
  // 4) SERIAL COMMANDS
  //    Handle single-character commands over the Serial port for playback control
  // ---------------------------------------------------------------------------
  if (Serial.available()) {
    char cmd = Serial.read();
    // ignore carriage returns / newlines
    if (cmd != '\n' && cmd != '\r') {
      // PLAYING ↔ PAUSED and STOP commands
      switch (state) {
        case STATE_PLAYING:
          if (cmd == 'p') {
            // Pause playback immediately
            player_stop_all();
            state = STATE_PAUSED;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex],
                                   (unsigned long)playTime,
                                   /* status= */1,
                                   tempoFactor,
                                   transposeValue);
            log_event("Paused");
          }
          else if (cmd == 's') {
            // Stop and return to file menu
            player_stop_all();
            state = STATE_MENU;
            oled_show_file_list(fileList, fileCount, selIndex);
            log_event("Stopped");
          }
          break;

        case STATE_PAUSED:
          if (cmd == 'p') {
            // Resume playback
            lastMillis = millis();
            state      = STATE_PLAYING;
            timeSinceLastRefresh = 0;
            oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                                   fileList[selIndex],
                                   (unsigned long)playTime,
                                   /* status= */0,
                                   tempoFactor,
                                   transposeValue);
            log_event("Resumed");
          }
          else if (cmd == 's') {
            // Stop and go back to menu
            player_stop_all();
            state = STATE_MENU;
            oled_show_file_list(fileList, fileCount, selIndex);
            log_event("Stopped");
          }
          break;

        default:
          // no action in other states
          break;
      }

      // rewind 5s (buffered)
      if (cmd == 'z') {
        pendingSeekDeltaMs -= 5000;
        lastSeekRequestMs  = millis();
        Serial.println(F("[CMD] Buffered rewind 5s"));
        log_event("Rewind 5s");
      }
      // fast-forward 5s (immediate)
      if (cmd == 'x') {
        playTime   += 5000.0;
        lastMillis  = millis();
        player_stop_all();
        Serial.println(F("[CMD] Forward 5s"));
        log_event("Forward 5s");
      }

      // tempo adjustment
      if (cmd == 'w') {
        tempoFactor += 0.1;
        Serial.print(F("[CMD] Tempo+ → ")); Serial.println(tempoFactor);
        log_event("Tempo +0.1");
      }
      if (cmd == 'q') {
        tempoFactor = max(0.1, tempoFactor - 0.1);
        Serial.print(F("[CMD] Tempo- → ")); Serial.println(tempoFactor);
        log_event("Tempo -0.1");
      }

      // transpose adjustment
      if (cmd == ']') {
        transposeValue++;
        player_modify_transpose(+1);
        Serial.println(F("[CMD] Transpose+"));
        log_event("Transpose +1");
      }
      if (cmd == '[') {
        transposeValue--;
        player_modify_transpose(-1);
        Serial.println(F("[CMD] Transpose-"));
        log_event("Transpose -1");
      }
    }
  }

  // ---------------------------------------------------------------------------
  // 5) BUFFERED SEEK
  //    Wait a short delay to batch multiple seek requests, then perform actual seek
  // ---------------------------------------------------------------------------
  if ((state == STATE_PLAYING || state == STATE_PAUSED)
      && pendingSeekDeltaMs != 0
      && (millis() - lastSeekRequestMs) >= SEEK_BUFFER_DELAY) {
    // compute new playback time, clamp to zero
    double nt = playTime + pendingSeekDeltaMs;
    nt = (nt < 0.0) ? 0.0 : nt;

    // re-position in file and resume at newTime
    player_seek((unsigned long)nt, fileList[selIndex]);
    playTime           = nt;
    lastMillis         = millis();
    pendingSeekDeltaMs = 0;
    log_event("Executed seek");

    // refresh playback menu to reflect new position
    oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                           fileList[selIndex],
                           (unsigned long)playTime,
                           (state == STATE_PLAYING ? 0 : 1),
                           tempoFactor,
                           transposeValue);
  }

  // ---------------------------------------------------------------------------
  // 6) PLAYER UPDATE LOOP
  //    Drive the playback engine and detect end of song
  // ---------------------------------------------------------------------------
  if (state == STATE_PLAYING) {
    // feed next note events to buzzers
    player_update((unsigned long)playTime);

    // periodically refresh UI (every ~9 seconds)
    if (timeSinceLastRefresh > 9000) {
      oled_show_playback_menu(playbackOpts, playbackCount, playSel,
                             fileList[selIndex],
                             (unsigned long)playTime,
                             /* status= */0,
                             tempoFactor,
                             transposeValue);
      timeSinceLastRefresh = 0;
    }

    // if file finished and no active notes, return to menu
    if (sd_finished() && player_is_idle()) {
      state = STATE_MENU;
      oled_show_file_list(fileList, fileCount, selIndex);
      log_event("End of song");
    }
  }
}
