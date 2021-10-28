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
#include "rom/ets_sys.h"

#include "FastLED.h"

//#include "ota.hpp"

#include "HomeLights.h"

static const char *TAG = "LIGHTS";

// To change build size
// static const char *OTA_FORCE = ""; //"test OTA string";

#define NUM_STRIPS 8

// SN74HCT245 OUTPUT_ENABLE, active_low
#define ENABLE_PIN GPIO_NUM_15

#define DATA_PIN_1 12
#define DATA_PIN_2 13
#define DATA_PIN_3 14
#define DATA_PIN_4 25
#define DATA_PIN_5 26
#define DATA_PIN_6 27
#define DATA_PIN_7 33
#define DATA_PIN_8 32


#define NUM_LEDS    50
#define BRIGHTNESS  64
#define STRAND_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_STRIPS][NUM_LEDS];

#define LIGHTS_CORE 0

void FASTLED_safe_show() {
  FastLED.show();

  // In theory could be as low as 50us, needed to not clobber next show
  // In practice 300us seems to work nicely
  ets_delay_us(300);
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

// Input a value 0 to 65,535 to get a color value.
// The colours are a transition r -> g -> b -> back to r.
CRGB cm_Wheel(uint16_t WheelPos) {

    // Maps WheelPos[0 -> 65536] -> [0, 3 * 255]
    uint8_t up = (((int) 3 * WheelPos) >> 8) & 255;
    uint8_t down = 255 - up;

    // 21845 is last 255
    // 43690 is 2nd last

    if (WheelPos <= 21845) {
        return CRGB(down, 0, up);
    }
    if (WheelPos <= 43690) {
        return CRGB(0, up, down);
    }
    return CRGB(up, down, 0);

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

    //int frame = 0;

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
        ESP_LOGI(TAG, "Strip %d", i);
        for (int j = 0; j <= i; j++) {
          gpio_set_level(GPIO_NUM_2, HIGH);
          vTaskDelay(pdMS_TO_TICKS(100));
          gpio_set_level(GPIO_NUM_2, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));
        }

        for (int j = 0; j < 2*NUM_LEDS; j++) {
          FastLED.clear();
          int k = j < NUM_LEDS ? j : (2*NUM_LEDS - j - 1);
          assert( (0 <= k) && (NUM_LEDS - 1) );
          leds[i][k] = cm_Wheel(i + j);
          FASTLED_safe_show();
        }

        FastLED.clear();
        FASTLED_safe_show();
        vTaskDelay(pdMS_TO_TICKS(500));
      }

/*
      for (int z = 0; z < 16 * 256; z++) {
        frame += 40;
        FastLED.clear();
        for (int i = 0; i < NUM_STRIPS; i++) {
          for (int j = 0; j < NUM_LEDS; j++) {
            leds[i][j] = cm_Wheel(frame + 100 * j);
          }
        }
        if (z % 256 == 0)
          vTaskDelay(pdMS_TO_TICKS(2000));

        FASTLED_safe_show();
      }
*/

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

  FastLED.addLeds<STRAND_TYPE, DATA_PIN_1, COLOR_ORDER>(leds[0], NUM_LEDS);
  if (NUM_STRIPS >= 2) FastLED.addLeds<STRAND_TYPE, DATA_PIN_2, COLOR_ORDER>(leds[1], NUM_LEDS);
  if (NUM_STRIPS >= 3) FastLED.addLeds<STRAND_TYPE, DATA_PIN_3, COLOR_ORDER>(leds[2], NUM_LEDS);
  if (NUM_STRIPS >= 4) FastLED.addLeds<STRAND_TYPE, DATA_PIN_4, COLOR_ORDER>(leds[3], NUM_LEDS);
  if (NUM_STRIPS >= 5) FastLED.addLeds<STRAND_TYPE, DATA_PIN_5, COLOR_ORDER>(leds[4], NUM_LEDS);
  if (NUM_STRIPS >= 6) FastLED.addLeds<STRAND_TYPE, DATA_PIN_6, COLOR_ORDER>(leds[5], NUM_LEDS);
  if (NUM_STRIPS >= 7) FastLED.addLeds<STRAND_TYPE, DATA_PIN_7, COLOR_ORDER>(leds[6], NUM_LEDS);
  if (NUM_STRIPS >= 8) FastLED.addLeds<STRAND_TYPE, DATA_PIN_8, COLOR_ORDER>(leds[7], NUM_LEDS);

  ESP_LOGI(TAG, "Limitting MAX power\n");
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);

  // Enable SN74HCT255 at this point
  gpio_set_level(ENABLE_PIN, LOW);

  pattern_test();
}

void home_lights_run(void *pvParameters) {
  hl_setup();
  hl_loop();
}


extern "C" {
  void app_main();
}

void app_main() {
  gpio_reset_pin(ENABLE_PIN);
  gpio_set_direction(ENABLE_PIN, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(ENABLE_PIN, GPIO_FLOATING);


  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_2, LOW);

  //ESP_LOGI(TAG, "Trying to establish OTA capability (%s)\n", OTA_FORCE);
  //setup_OTA_task();

  vTaskDelay(pdMS_TO_TICKS(1000));

 // ESP_LOGI(TAG, "Creating task for run_lights()\n");
 // xTaskCreate(&run_lights, "blinkLeds", 4000, (void*) NULL, 5, NULL);

  ESP_LOGI(TAG, "Creating task for home_lights()\n");
  xTaskCreate(&home_lights_run, "home lights", 4000, (void*) NULL, 5, NULL);

}