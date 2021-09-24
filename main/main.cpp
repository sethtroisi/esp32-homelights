/* Combined FastLED-idf and OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "FastLED.h"

#include "ota.h"

#define NUM_STRIPS 1

#define DATA_PIN_1 26
#define DATA_PIN_2 25
#define DATA_PIN_3 33
#define DATA_PIN_4 32

#define NUM_LEDS    150
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B

CRGB leds[NUM_STRIPS][NUM_LEDS];

void clear_long() {
    for(auto c = CLEDController::head(); c != nullptr; c = c->next()) {
        c->clearLeds(300);
    }

    FastLED.clear();
    FastLED.show();
    delay(2); // Needed so as not to keep writing past end of NUM_LEDS if called quickly again.
}

void FASTLED_safe_show() {
  delay(2); // Avoid potential interference with past show()
  FastLED.show();
}

void pattern_test(void *pvParameters) {
  CRGB colors[] = {
    CRGB::Green,
    CRGB::RoyalBlue,
    CRGB::Violet,
    CRGB::FairyLightNCC,
    CRGB::SpringGreen
  };

  while(1){
    printf("pattern test loop\n");
    //clear_long();

    // Run down and back
    while (1) {
      for (int i = 0; i < NUM_STRIPS; i++) {
        for (int j = 0; j < 2 * NUM_LEDS; j++) {
          FastLED.clear();
          int k = (NUM_LEDS-1) - abs((NUM_LEDS-1) - j);
          leds[i][k] = colors[i];
          FASTLED_safe_show();
          delay(10);
        }
      }

      printf("Here\n");
      delay(500);
      digitalWrite(2, HIGH);
      delay(500);
      digitalWrite(2, LOW);
    }

    // Run down each strip individual
    for (int i = 0; i < NUM_STRIPS; i++) {
	    for (int j = 0; j < NUM_LEDS; j++) {
        FastLED.clear();
	      leds[i][j] = colors[i];
        FASTLED_safe_show();
	    }

      digitalWrite(2, HIGH);
      FastLED.clear();
      uint64_t start = esp_timer_get_time();
      FastLED.show();
      uint64_t end = esp_timer_get_time();
      printf("Show Time: %" PRIu64 "\n",end-start);
      delay(1000);
      digitalWrite(2, LOW);
    }

    // Run pattern on each strip
    for (int l = 0; l < 5; l++) {
      for (int j = 0; j < NUM_LEDS; j++) {
        FastLED.clear();
        for (int i = 0; i < NUM_STRIPS; i++) {
          leds[i][j] = colors[i];
        }
        FASTLED_safe_show();
      }

      digitalWrite(2, HIGH);
      FastLED.clear();
      uint64_t start = esp_timer_get_time();
      FastLED.show();
      uint64_t end = esp_timer_get_time();
      printf("Show Time: %" PRIu64 "\n",end-start);
      delay(1000);
      digitalWrite(2, LOW);
    }
	 };
}

extern "C" {
  void app_main();
}

void app_main() {
  TaskHandle_t *xHandle = NULL;

  printf("Trying to establish OTA capability\n");
  setup_and_poll_ota(xHandle);
  delay(1000);

  printf("Calling addLeds<>()\n");

  FastLED.addLeds<LED_TYPE, DATA_PIN_1>(leds[0], NUM_LEDS);
  if (NUM_STRIPS >= 2) FastLED.addLeds<LED_TYPE, DATA_PIN_2>(leds[1], NUM_LEDS);
  if (NUM_STRIPS >= 3) FastLED.addLeds<LED_TYPE, DATA_PIN_3>(leds[2], NUM_LEDS);
  if (NUM_STRIPS >= 4) FastLED.addLeds<LED_TYPE, DATA_PIN_4>(leds[3], NUM_LEDS);

  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

  printf("Limitting MAX power\n");
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

  // change the task below to one of the functions above to try different patterns
  printf("Creating task for led testing\n");

  xTaskCreatePinnedToCore(&pattern_test, "blinkLeds", 4000, NULL, 5, xHandle, 0);
}