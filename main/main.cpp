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
#include "esp_task_wdt.h"

#include "FastLED.h"

#include "ota.hpp"

static const char *TAG = "LIGHTS";

// To change build size
static const char *OTA_FORCE = "" "extra even longer very long string why did I do this to myself";

#define NUM_STRIPS 1

#define DATA_PIN_1 26
#define DATA_PIN_2 25
#define DATA_PIN_3 33
#define DATA_PIN_4 32

#define NUM_LEDS    150
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_STRIPS][NUM_LEDS];

#define LIGHTS_CORE 0

void FASTLED_safe_show() {
  // Setting this < ~10ms means this pattern_test is a busy loop
  // That requires disabling Watch CPU1 (LIGHTS_CORE) Idle Task
  // menuconfig -> Component config -> ESP System Settings

  // Could be as low as 50us, needed to not clobber next show
  vTaskDelay(pdMS_TO_TICKS(10));
  FastLED.show();
}

/**
 * Ugly hack
 * 1. xTaskGetHandle isn't available
 * 2. Other solutions often have xTaskGetHandle go out of scope
 */
extern TaskHandle_t update_task;


void clear_long() {
    for(auto c = CLEDController::head(); c != nullptr; c = c->next()) {
        c->clearLeds(300);
    }

    FastLED.clear();
    FASTLED_safe_show();
}

void pattern_test(void) {
  CRGB colors[] = {
    CRGB::BlueViolet,
  };

  // We aint need no watchdog. alternatively call esp_task_wdt_reset() in FASTLED_safe_show()
  //esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

  // digitalWrite(2, HIGH);
  // FastLED.clear();
  // uint64_t start = esp_timer_get_time();
  // FastLED.show();
  // uint64_t end = esp_timer_get_time();
  // ESP_LOGI(TAG, "Show Time: %" PRIu64, end - start);
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // digitalWrite(2, LOW);


  while(1){
    ESP_LOGI(TAG, "Starting pattern_test() loop\n");
    //clear_long();

    // Run down and back
    while (1) {
      // Take semaphore (this is ugly because we can't take from another task)
      // uint32_t semaphore = 10;
      // while (1) {
      //   xTaskNotifyAndQuery(update_task, 0, eSetValueWithOverwrite, &semaphore);
      //   assert( semaphore <= 1 );
      //   if (semaphore == 1) {
      //     break;
      //   }
      //   ESP_LOGI(TAG, "Waiting on semaphore");
      //   vTaskDelay(pdMS_TO_TICKS(1000));
      // }

      ESP_LOGI(TAG, "Run");

      for (int i = 0; i < NUM_STRIPS; i++) {
        ESP_LOGI(TAG, "#1 %d", i);
        for (int j = 0; j < 2*NUM_LEDS; j++) {
          if (j % 4 == 0) {
            ESP_LOGI(TAG, " #1 %d-%d", i, j);
            vTaskDelay(pdMS_TO_TICKS(10));
          }
          FastLED.clear();
          int k = j < NUM_LEDS ? j : (2*NUM_LEDS - j - 1);
          assert( (0 <= k) && (NUM_LEDS - 1) );
          leds[i][k] = colors[i];
          FASTLED_safe_show();
        }
      }
      FastLED.clear();
      FASTLED_safe_show();

      ESP_LOGI(TAG, "Stop");
      //xTaskNotifyGive(update_task);
      vTaskDelay(pdMS_TO_TICKS(1000));
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
  vTaskDelay(pdMS_TO_TICKS(1000));

  // change the task below to one of the functions above to try different patterns
  ESP_LOGI(TAG, "Creating task for run_lights()\n");
  xTaskCreatePinnedToCore(&run_lights, "blinkLeds", 4000, (void*) NULL, 5, NULL, LIGHTS_CORE);
}