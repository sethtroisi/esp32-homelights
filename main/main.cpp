/* Combined FastLED-idf and OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cassert>
#include <cstdio>
#include <cstdint>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_system.h>

#include "HomeLights.h"

#define FASTLED_ESP32_I2S
#include <FastLED.h>
FASTLED_USING_NAMESPACE


static const char *TAG = "LIGHTS";


void home_lights_run(void *pvParameters) {
  hl_setup();

  while (1) {
    hl_loop();
  }

  // In the weird that we end kill our task
  ESP_LOGI(TAG, "HOME LIGHTS RUN ENDED?\n");
  vTaskDelete(NULL);
}

extern "C" {
  void app_main();
}

void app_main() {

  /**
   * Tasks must never end:
   * From https://www.freertos.org/a00125.html
   *
   * "Tasks are normally implemented as an infinite loop;
   *  the function which implements the task must never attempt to return or exit.
   *  Tasks can, however, delete themselves."
   */

  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "Creating task for home_lights()\n");
  xTaskCreate(&home_lights_run, "home lights", /*usStackDepth=*/ 4000, (void*) NULL, /*uxPriority=*/ 5, NULL);

  size_t i = 0;
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    // ESP_LOGI(TAG, "MAIN LOOP %u\n", ++i);
  }
}
