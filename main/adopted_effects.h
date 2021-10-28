#pragma once

//---------------------------------------------------------------------------||
//----------------------------- Adopted effects -----------------------------//

/*
 * Inspiration from these and others
 * https://github.com/FastLED/FastLED/blob/master/examples/
 * https://docs.wled.me/features/effects-palettes/
 * https://github.com/Aircoookie/WLED/blob/master/wled00/FX.cpp#L3793
 */



#include "globals.h"

// Forward definitions
CRGB ColorMap(uint16_t WheelPos);
uint32_t guessWheel(CRGB guess);
void setStrip(CRGB color);
void setPixel(short pixel, CRGB color);
void setSinglePixel(uint8_t strip_i, short pixel, CRGB color);
void showStrips();
void ripples_pattern(CRGB ripple_color, bool wander_color);


//---------------------------------------------------------------------------||

void pride()
{
    // https://github.com/FastLED/FastLED/blob/master/examples/Pride2015/Pride2015.ino

    // This function draws rainbows with an ever-changing,
    // widely-varying set of parameters.

  static uint16_t sPseudotime = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t deltams = global_tDelta * 1000;
  sPseudotime += deltams * msmultiplier;

  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;

  for( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    // Want negative led
    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
    setPixel(pixelnumber, leds[pixelnumber]);
  }

  showStrips();
}

void lightningSetup() {
  lightning_effect.remaining_strikes = 0;
  lightning_effect.off_time = 0;
}

void lightning()
{
  // Strongly Adapted from https://github.com/Aircoookie/WLED/blob/7019ddb1650eb77cd11bf29d6780ed892c4df64a/wled00/FX.cpp#L1613

   // Setup next strike
  if (lightning_effect.off_time < global_t) {
    lightning_effect.start_time = global_t;
    lightning_effect.on_time = global_t + .2; // 200ms delay after leader
    lightning_effect.off_time = lightning_effect.on_time + 0.2 + .001 * random(100);  // 200 - 300 ms off

    lightning_effect.is_leader = (lightning_effect.remaining_strikes == 0);
    if (lightning_effect.is_leader)
    {  // Setup new group of strikes
      lightning_effect.remaining_strikes = 4 + random(global_s / 20);  //number of flashes     
    }
    // Leader strike is brighter
    lightning_effect.brightness = (128 / (1 + random(2))); 

    lightning_effect.remaining_strikes -= 1;
    if (lightning_effect.remaining_strikes == 0) {
      lightning_effect.off_time += (30 + random(30)) * .1; // 3 - 6 seconds before starting lightning effect again (timing seems to be wrong)
    }

    // Determine start & length (of lightning flash)
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
      uint16_t led_start = random(NUM_LEDS - 10);
      lightning_effect.start_led[strip_i] = led_start;
      lightning_effect.end_led[strip_i] = led_start + std::min(50, 1 + random(NUM_LEDS - led_start));      
    }

  }

  // How to Toggle background between 
  if (current_pattern == LIGHTNING) {
    // Reset to black
    setStrip(CRGB::Black);
  } else {
    // TODO LIGHTNING_RIPPLES
    ripples_pattern(CRGB(0, 10, 100), false); // Blue background for our storm
  }

  // Check if lightening "on" or "off"
  if (global_t < lightning_effect.on_time) {
    // Set white / slightly blue
    uint8_t bri = lightning_effect.brightness;
    uint8_t blue_bri = bri < 220 ? bri + 30 : bri;
    CRGB lightning_color = lightning_effect.is_leader ?
        CRGB(bri, bri, 255) : CRGB(bri, bri, blue_bri);

    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
      if ((active_strips & (1 << strip_i)) == 0)
        continue;

      // Flash on odd numbers >= 3
      for (int led_i = lightning_effect.start_led[strip_i]; led_i < lightning_effect.end_led[strip_i]; led_i++)
        setSinglePixel(strip_i, led_i, lightning_color);
    }
  }
}