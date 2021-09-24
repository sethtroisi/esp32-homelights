/* Combined FastLED-idf and OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cassert>
#include <cstdio>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "FastLED.h"

#include "ota.h"

static const char *TAG = "LIGHTS";

// To change build size
static const char *OTA_FORCE = "extra even longer very long string why did I do this to myself";

#define NUM_STRIPS 1

#define DATA_PIN_1 26
#define DATA_PIN_2 25
#define DATA_PIN_3 33
#define DATA_PIN_4 32

#define NUM_LEDS    50
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

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

void pattern_test(void) {
  CRGB colors[] = {
    CRGB::Red,
  };

  //TaskHandle_t updateTask = xTaskGetHandle(OTA_TASK_NAME);

  // digitalWrite(2, HIGH);
  // FastLED.clear();
  // uint64_t start = esp_timer_get_time();
  // FastLED.show();
  // uint64_t end = esp_timer_get_time();
  // ESP_LOGI(TAG, "Show Time: %" PRIu64, end - start);
  // delay(1000);
  // digitalWrite(2, LOW);


  while(1){
    ESP_LOGI(TAG, "Starting pattern_test() loop\n");
    //clear_long();

    // Run down and back
    while (1) {
      // Take semaphore (this is ugly because we can't take from another task)
      // uint32_t semaphore = 10;
      // while (1) {
      //   xTaskNotifyAndQuery(updateTask, 0, eSetValueWithOverwrite, &semaphore);
      //   assert( semaphore <= 1 );
      //   if (semaphore == 1) {
      //     break;
      //   }
      //   printf("LIGHTS Waiting on semaphore\n");
      //   delay(500);
      // }

      ESP_LOGI(TAG, "Run");

      for (int i = 0; i < NUM_STRIPS; i++) {
        for (int j = 0; j < 2*NUM_LEDS; j++) {
          FastLED.clear();
          int k = j < NUM_LEDS ? j : (2*NUM_LEDS - j - 1);
          assert( (0 <= k) && (k <= (NUM_LEDS - 1)) );
          leds[i][k] = colors[i];
          FASTLED_safe_show();
          delay(10);
        }
      }
      FastLED.clear();
      FASTLED_safe_show();

      ESP_LOGI(TAG, "Stop");
//      xTaskNotifyGive(updateTask);
      delay(1000);
    }

    // Run down each strip individual
    for (int i = 0; i < NUM_STRIPS; i++) {
	    for (int j = 0; j < NUM_LEDS; j++) {
        FastLED.clear();
	      leds[i][j] = colors[i];
        FASTLED_safe_show();
	    }
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
    }
	 }
}

void run_lights(void *pvParameters) {
  ESP_LOGI(TAG, "Calling addLeds<>()\n");

  FastLED.addLeds<LED_TYPE, DATA_PIN_1, COLOR_ORDER>(leds[0], NUM_LEDS);
  if (NUM_STRIPS >= 2) FastLED.addLeds<LED_TYPE, DATA_PIN_2, COLOR_ORDER>(leds[1], NUM_LEDS);
  if (NUM_STRIPS >= 3) FastLED.addLeds<LED_TYPE, DATA_PIN_3, COLOR_ORDER>(leds[2], NUM_LEDS);
  if (NUM_STRIPS >= 4) FastLED.addLeds<LED_TYPE, DATA_PIN_4, COLOR_ORDER>(leds[3], NUM_LEDS);

  ESP_LOGI(TAG, "Limitting MAX power\n");
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

  pattern_test();
}


extern "C" {
  void app_main();
}

void app_main() {
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_2, LOW);

  ESP_LOGI(TAG, "Trying to establish OTA capability (%s)\n", OTA_FORCE);

  //setup_OTA_task();
  delay(1000);

  // change the task below to one of the functions above to try different patterns
  ESP_LOGI(TAG, "Creating task for run_lights()\n");
  xTaskCreatePinnedToCore(&run_lights, "blinkLeds", 4000, (void*) NULL, 5, NULL, 0);
}