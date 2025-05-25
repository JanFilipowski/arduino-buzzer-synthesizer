#include "oled_gui.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// -----------------------------------------------------------------------------
// Display pin definitions
// -----------------------------------------------------------------------------
#define TFT_CS   8   // Chip Select
#define TFT_DC   12  // Data/Command
#define TFT_RST  11  // Reset
#define TFT_BL    7  // Backlight

// Single instance of the ST7735 display driver
static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// -----------------------------------------------------------------------------
// Icon drawing helpers (all in white)
// -----------------------------------------------------------------------------

/**
 * @brief Draw an upward-pointing triangle arrow at (x,y).
 */
static void drawArrowUp(int x, int y) {
  tft.fillTriangle(x, y+6, x+6, y+6, x+3, y, ST77XX_WHITE);
}

/**
 * @brief Draw a downward-pointing triangle arrow at (x,y).
 */
static void drawArrowDown(int x, int y) {
  tft.fillTriangle(x, y, x+6, y, x+3, y+6, ST77XX_WHITE);
}

/**
 * @brief Draw a right-pointing triangle arrow at (x,y).
 */
static void drawArrowRight(int x, int y) {
  tft.fillTriangle(x, y, y+6, y+6, x+6, y+3, ST77XX_WHITE);
}

// (Optional left arrow not used)
// static void drawArrowLeft(int x, int y) { … }

// -----------------------------------------------------------------------------
// oled_init()
//   - Enable backlight
//   - Initialize the display controller
//   - Set rotation and clear screen
// -----------------------------------------------------------------------------
void oled_init() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);      // Turn on backlight

  tft.initR(INITR_BLACKTAB);       // Initialize ST7735 with black tab
  tft.setRotation(1);              // Landscape mode
  tft.fillScreen(ST77XX_BLACK);    // Clear to black
}

// -----------------------------------------------------------------------------
// oled_show_file_list()
//   Display a scrollable list of filenames, highlighting the selected entry.
//   - list[]: array of strings (filenames without “.csv” suffix trimmed by caller)
//   - count:  number of items in list[]
//   - sel:    index of currently highlighted item
// -----------------------------------------------------------------------------
void oled_show_file_list(const char* list[], uint8_t count, uint8_t sel) {
  const uint8_t HEADER_H = 24;            // Height reserved for title
  const uint8_t FH       = 16;            // Font height in pixels
  uint8_t pageSize = (tft.height() - HEADER_H) / FH;
  static int pageStart = 0;               // Index of topmost visible entry

  // Adjust scrolling window to include sel
  if (sel < pageStart) {
    pageStart = sel;
  } else if (sel >= pageStart + pageSize) {
    pageStart = sel - pageSize + 1;
  }

  tft.fillScreen(ST77XX_BLACK);

  // Draw header text
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 4);
  tft.print(F("PLAYLIST"));

  // Draw each visible filename
  for (uint8_t i = 0; i < pageSize; i++) {
    uint8_t idx = pageStart + i;
    if (idx >= count) break;
    int16_t y = HEADER_H + i * FH;

    // Highlight selected line with an arrow on the right
    if (idx == sel) {
      tft.setCursor(tft.width() - 20, y);
      tft.write('<');
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    // Trim “.csv” suffix if present
    const char* name = list[idx];
    size_t len = strlen(name);
    size_t dispLen = (len > 4 && strcasecmp(name + len - 4, ".csv") == 0) ? len - 4 : len;

    tft.setCursor(35, y);
    tft.write((const uint8_t*)name, dispLen);
  }

  // Draw scroll icons on the left margin
  int16_t iconX = 2;
  drawArrowUp(iconX, iconX + 10);
  drawArrowRight(iconX, tft.height() / 2 - 5);
  drawArrowDown(iconX, tft.height() - 20);
}

// -----------------------------------------------------------------------------
// oled_show_paused()
//   Clear screen and display “PAUSED” centered.
// -----------------------------------------------------------------------------
void oled_show_paused() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height() / 2 - 8);
  tft.print(F("PAUSED"));
}

// -----------------------------------------------------------------------------
// oled_show_loading()
//   Clear screen and display “Loading...” centered.
// -----------------------------------------------------------------------------
void oled_show_loading() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height() / 2 - 8);
  tft.print(F("Loading..."));
}

// -----------------------------------------------------------------------------
// oled_show_error(msg)
//   Clear screen and show error message centered.
// -----------------------------------------------------------------------------
void oled_show_error(const char* msg) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height() / 2 - 16);
  tft.print(F("ERROR:"));
  tft.setCursor(20, tft.height() / 2);
  tft.print(msg);
}

// -----------------------------------------------------------------------------
// oled_show_playback_menu()
//   Render the playback controls and status info.
//   - opts[]:    array of icons/labels for each control
//   - count:     number of options
//   - sel:       index of highlighted control
//   - filename:  current file name (trimmed by caller)
//   - playertime: elapsed playback time in ms
//   - status:    0 = playing, 1 = paused
//   - tempo:     current speed multiplier
//   - transpose: semitone offset (signed)
// -----------------------------------------------------------------------------
void oled_show_playback_menu(const char* opts[], uint8_t count,
                             uint8_t sel, const char* filename,
                             unsigned long playertime, unsigned long status,
                             double tempo, long transpose) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);

  // Draw vertical list of control icons
  for (uint8_t i = 0; i < count; i++) {
    int16_t y = i * 16;
    if (i == sel) {
      // Highlight background for selected item
      tft.fillRect(0, y, 30, 16, ST77XX_WHITE);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    // If paused, show a “>” on Play icon
    if (status && i == 0) {
      tft.setCursor(10, 2);
      tft.print('>');
      continue;
    }

    tft.setCursor(4, y + 2);
    tft.print(opts[i]);
  }

  // Display filename (trim “.csv” if present)
  tft.setTextColor(ST77XX_WHITE);
  size_t len     = strlen(filename);
  size_t dispLen = (len > 4 && strcasecmp(filename + len - 4, ".csv") == 0) ? len - 4 : len;
  tft.setCursor(tft.width() - (dispLen * 6), 10);
  tft.write((const uint8_t*)filename, dispLen);

  // Show paused status text
  tft.setCursor(tft.width() - 100, 30);
  if (status) {
    tft.print("Paused");
  }

  // Compute minutes:seconds from milliseconds
  unsigned long secs = playertime / 1000;
  unsigned long mins = secs / 60;
  secs = secs % 60;

  // Draw time
  tft.setCursor(tft.width() - 100, 65);
  tft.print(mins);
  tft.print(':');
  if (secs < 10) tft.print('0');
  tft.print(secs);

  // Draw tempo and transpose values
  tft.setCursor(tft.width() - 100, 90);
  tft.print("S: ");
  tft.print(tempo);
  tft.setCursor(tft.width() - 100, 105);
  tft.print("T: ");
  if (transpose >= 0) tft.print('+');
  tft.print(transpose);
}
