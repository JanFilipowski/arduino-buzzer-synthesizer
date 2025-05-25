#include "oled_gui.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS  8
#define TFT_DC 12
#define TFT_RST 11
#define TFT_BL   7

static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


// proste ikonki
static void drawArrowUp(int x, int y) {
  tft.fillTriangle(x, y+6, x+6, y+6, x+3, y, ST77XX_WHITE);
}
static void drawArrowDown(int x, int y) {
  tft.fillTriangle(x, y, x+6, y, x+3, y+6, ST77XX_WHITE);
}
static void drawArrowRight(int x, int y) {
  tft.fillTriangle(x, y, x, y+6, x+6, y+3, ST77XX_WHITE);
}

/* STRZAŁKA W LEWO (NIEUŻYWANA)
static void drawArrowLeft(int x, int y) {
  tft.fillTriangle(x+6, y, x+6, y+6, x, y+3, ST77XX_WHITE);
}*/


void oled_init() {
  // backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
}

void oled_show_file_list(const char* list[], uint8_t count, uint8_t sel) {
  const uint8_t HEADER_H = 24;
  const uint8_t FH       = 16;
  uint8_t pageSize = (tft.height() - HEADER_H) / FH;
  static int pageStart = 0;

  // przesuwamy okno jeśli poza zakresem
  if (sel < pageStart)           pageStart = sel;
  else if (sel >= pageStart + pageSize)
                                 pageStart = sel - pageSize + 1;

  // tło
  tft.fillScreen(ST77XX_BLACK);

  // nagłówek
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 4);
  tft.print(F("PLAYLIST"));

  // lista plików
  for (uint8_t i = 0; i < pageSize; i++) {
    uint8_t idx = pageStart + i;
    if (idx >= count) break;
    int16_t y = HEADER_H + i * FH;
    if (idx == sel) {
      // tło zaznaczenia
      //tft.fillRect(0, y, tft.width(), FH, ST77XX_WHITE);
      //tft.setTextColor(ST77XX_BLACK);
        
      //drawArrowLeft(tft.width() - 10, y);
      tft.setCursor(tft.width()-20, y);
      tft.write("<");

    } else {
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(35, y);
    
    const char* name = list[idx];
    size_t len     = strlen(name);
    size_t dispLen = (len > 4) ? len - 4 : len;

    tft.write((const uint8_t*)name, dispLen);

  }

  // ikonki po lewej
  int16_t iconX = 2;
  drawArrowUp(iconX, iconX+10);
  drawArrowRight   (iconX, tft.height()/2 - 5);
  drawArrowDown  (iconX, tft.height() - 20);
}


void oled_show_paused() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height()/2 - 8);
  tft.print(F("PAUSED"));
}

void oled_show_loading() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height()/2 - 8);
  tft.print(F("Loading..."));
}

void oled_show_error(const char* msg) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, tft.height()/2 - 16);
  tft.print(F("ERROR:"));
  tft.setCursor(20, tft.height()/2);
  tft.print(msg);
}

void oled_show_playback_menu(const char* opts[], uint8_t count, uint8_t sel, const char* filename, unsigned long playertime, unsigned long status, double tempo, long transpose) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);

  for (uint8_t i = 0; i < count; i++) {
    int16_t y = i*16;
    if (i==sel) {
      tft.fillRect(0, y, 30, 16, ST77XX_WHITE);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    if (status) if(i == 0) {
        tft.setCursor(10, 2);
        tft.print('>');
        continue;
    }

    tft.setCursor(4, y+2);
    tft.print(opts[i]);
  }
  
    tft.setTextColor(ST77XX_WHITE);
    size_t len     = strlen(filename);
    size_t dispLen = (len > 4) ? len - 4 : len;

    tft.setCursor(tft.width()-len*10, 10);
    tft.write((const uint8_t*)filename, dispLen);

  tft.setCursor(tft.width()-100, 30);
  if (status)
    tft.print("Paused");

  if ((long)playertime < 0 || playertime > 6000000) playertime = 0;
  unsigned long calculateMinutes, calculateSeconds = playertime / 1000;
  calculateMinutes = calculateSeconds / 60;
  calculateSeconds = calculateSeconds % 60;

  // wyświetlanie minut
  tft.setCursor(tft.width()-100, 65);
  tft.print(calculateMinutes);
  tft.print(':');
  if (calculateSeconds < 10) tft.print(0);
  tft.print(calculateSeconds);
  
  // wyświetlanie transpozycji i szybkości
  tft.setCursor(tft.width()-100, 90);
  tft.print("S: ");
  tft.print(tempo);
  tft.setCursor(tft.width()-100, 105);
  tft.print("T: ");
  if (transpose >= 0) tft.print('+');
  tft.print(transpose);
}
