// This sketch is for the RP2040 and ILI9341 TFT display.
// The RP2040 has sufficient RAM for a full screen buffer:
// 240 x 320 x 2 = 153,600 bytes
// In this example two sprites are used to create DMA toggle
// buffers. Each sprite is half the screen size, this allows
// graphics to be rendered in one sprite at the same time
// as the other sprite is being sent to the screen.

// Created by Bodmer 20/04/2021 as an example for:
// https://github.com/Bodmer/TFT_eSPI

/*
 * normal fps (17.85) on Pi Pico (30cm SPI wires) -  (89mA consumption):
 * set SPI_FREQUENCY  27000000 in eSPI User_Setup.h
 * Compile with CPU Speed: 133 MHz (default)
 * and without clock_configure(...) in setup() !!!
 * 
 * to get maximal fps (46.5) at Pi Pico (30cm SPI wires) -  (112mA consumption):
 * set SPI_FREQUENCY  62500000 in eSPI User_Setup.h
 * Compile with CPU Speed: 250 MHz (Overcklock)
 * and clock_configure(...) in setup() !!!
 * 
 * on ESP-WROOM-32 (30cm SPI wires):
 * set SPI_FREQUENCY  20000000 in eSPI User_Setup.h
 * runs at 16.25fps no matter of Espresif System - tested on ver 1.0.6 and 2.0.11
 * 
 * on ESP-WROOM-32 (20cm SPI wires):
 * SPI_FREQUENCY  27000000 - runs at 21.65fps - tested on Espresif System ver 2.0.11 (150mA consumption)
 * SPI_FREQUENCY  40000000 - runs at 32.46fps - SPI not working
 * SPI_FREQUENCY  62500000 - runs at 64.72fps - SPI not working
 * SPI_FREQUENCY  80000000 - runs at 64.72fps - SPI not working (200mA consumption)
*/

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_MBED_RP2040) && !defined(ARDUINO_ARCH_RP2040)
#pragma message("Unsupported platform")
#endif


// Number of circles to draw
#define CNUMBER 42

#include <TFT_eSPI.h>

// Library instance
TFT_eSPI    tft = TFT_eSPI();

// Create two sprites for a DMA toggle buffer
TFT_eSprite spr[2] = {TFT_eSprite(&tft), TFT_eSprite(&tft)};

// Pointers to start of Sprites in RAM
uint16_t* sprPtr[2];

// Used for fps measuring
uint16_t counter = 0;
int32_t startMillis = millis();
uint16_t interval = 100;
String fps = "xx.xx fps";

// Structure to hold circle plotting parameters
typedef struct circle_t {
  int16_t   cx[CNUMBER] = { 0 };
  int16_t   cy[CNUMBER] = { 0 };
  int16_t   cr[CNUMBER] = { 0 };
  uint16_t col[CNUMBER] = { 0 };
  int16_t   dx[CNUMBER] = { 0 };
  int16_t   dy[CNUMBER] = { 0 };
} circle_param;

// Create the structure and get a pointer to it
circle_t *circle = new circle_param;

// #########################################################################
// Setup
// #########################################################################
void setup() {
  /* 
   * Use with Pi Pico only!!!!!!!!!!!!!!!!!!!!!!!
   * Overclocking SPI to 62.5MHz at CPU clock 125MHz thanks to 
   * https://github.com/Bodmer/TFT_eSPI/issues/1460#issuecomment-1006661452
   * display drawing speeds up to 40 - 46 fps
  */

#if defined(TARGET_RP2040)
  uint32_t freq = clock_get_hz(clk_sys);
  // clk_peri does not have a divider, so in and out frequencies must be the same
  clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    freq,
                    freq);
#endif

  Serial.begin(115200);

  tft.init();

  tft.initDMA();

  tft.fillScreen(TFT_BLACK);

  // Create the 2 sprites, each is half the size of the screen
  sprPtr[0] = (uint16_t*)spr[0].createSprite(tft.width(), tft.height() / 2);
  sprPtr[1] = (uint16_t*)spr[1].createSprite(tft.width(), tft.height() / 2);

  // Define text datum for each Sprite
  spr[0].setTextDatum(MC_DATUM);
  spr[1].setTextDatum(MC_DATUM);

  // Initialise circle parameters
  for (uint16_t i = 0; i < CNUMBER; i++) {
    circle->cr[i] = random(12, 24);
    circle->cx[i] = random(circle->cr[i], tft.width() - circle->cr[i]);
    circle->cy[i] = random(circle->cr[i], tft.height() - circle->cr[i]);
    circle->col[i] = rainbow(4 * i);
    circle->dx[i] = random(1, 5);
    if (random(2)) circle->dx[i] = -circle->dx[i];
    circle->dy[i] = random(1, 5);
    if (random(2)) circle->dx[i] = -circle->dx[i];
  }

  tft.startWrite(); // TFT chip select held low permanently

  startMillis = millis();
}

// #########################################################################
// Loop
// #########################################################################
void loop() {
  drawUpdate(0); // Update top half
  drawUpdate(1); // Update bottom half

  //delay(1000);
  counter++;
  // only calculate the fps every <interval> iterations.
  if (counter % interval == 0) {
    long millisSinceUpdate = millis() - startMillis;
    fps = String((interval * 1000.0 / (millisSinceUpdate))) + " fps";
    Serial.println(fps);
    startMillis = millis();
  }
}

// #########################################################################
// Render circles to sprite 0 or 1 and initiate DMA
// #########################################################################
void drawUpdate (bool sel) {
  spr[sel].fillSprite(TFT_BLACK);
  for (uint16_t i = 0; i < CNUMBER; i++) {
    int16_t dsy = circle->cy[i] - (sel * tft.height() / 2);
    // Draw
    spr[sel].fillCircle(circle->cx[i], dsy, circle->cr[i], circle->col[i]);
    spr[sel].drawCircle(circle->cx[i], dsy, circle->cr[i], TFT_WHITE);
    spr[sel].setTextColor(TFT_BLACK, circle->col[i]);
    spr[sel].drawNumber(i + 1, 1 + circle->cx[i], dsy, 2);

    // Update circle positions after bottom half has been drawn
    if (sel) {
      circle->cx[i] += circle->dx[i];
      circle->cy[i] += circle->dy[i];
      if (circle->cx[i] <= circle->cr[i]) {
        circle->cx[i] = circle->cr[i];
        circle->dx[i] = -circle->dx[i];
      }
      else if (circle->cx[i] + circle->cr[i] >= tft.width() - 1) {
        circle->cx[i] = tft.width() - circle->cr[i] - 1;
        circle->dx[i] = -circle->dx[i];
      }
      if (circle->cy[i] <= circle->cr[i]) {
        circle->cy[i] = circle->cr[i];
        circle->dy[i] = -circle->dy[i];
      }
      else if (circle->cy[i] + circle->cr[i] >= tft.height() - 1) {
        circle->cy[i] = tft.height() - circle->cr[i] - 1;
        circle->dy[i] = -circle->dy[i];
      }
    }
  }
  tft.pushImageDMA(0, sel * tft.height() / 2, tft.width(), tft.height() / 2, sprPtr[sel]);
}

// #########################################################################
// Return a 16 bit rainbow colour
// #########################################################################
uint16_t rainbow(byte value)
{
  // If 'value' is in the range 0-159 it is converted to a spectrum colour
  // from 0 = red through to 127 = blue to 159 = violet
  // Extending the range to 0-191 adds a further violet to red band

  value = value % 192;

  byte red   = 0; // Red is the top 5 bits of a 16 bit colour value
  byte green = 0; // Green is the middle 6 bits, but only top 5 bits used here
  byte blue  = 0; // Blue is the bottom 5 bits

  byte sector = value >> 5;
  byte amplit = value & 0x1F;

  switch (sector)
  {
    case 0:
      red   = 0x1F;
      green = amplit; // Green ramps up
      blue  = 0;
      break;
    case 1:
      red   = 0x1F - amplit; // Red ramps down
      green = 0x1F;
      blue  = 0;
      break;
    case 2:
      red   = 0;
      green = 0x1F;
      blue  = amplit; // Blue ramps up
      break;
    case 3:
      red   = 0;
      green = 0x1F - amplit; // Green ramps down
      blue  = 0x1F;
      break;
    case 4:
      red   = amplit; // Red ramps up
      green = 0;
      blue  = 0x1F;
      break;
    case 5:
      red   = 0x1F;
      green = 0;
      blue  = 0x1F - amplit; // Blue ramps down
      break;
  }

  return red << 11 | green << 6 | blue;
}
