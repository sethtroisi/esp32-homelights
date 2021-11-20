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
#include "Networking.h"


//extern SemaphoreHandle_t xMutex;
SemaphoreHandle_t xMutex;

static const char *TAG = "LIGHTS";

// SN74HCT245 OUTPUT_ENABLE, active_low
#define LIGHTS_DISABLE_PIN GPIO_NUM_15

void home_lights_run(void *pvParameters) {
  hl_setup();

  while (1) {
    xSemaphoreTake( xMutex, portMAX_DELAY);
    hl_loop();
    xSemaphoreGive( xMutex );
  }

  // In the weird that we end kill our task
  ESP_LOGI(TAG, "HOME LIGHTS RUN ENDED?\n");
  vTaskDelete(NULL);
}

void recieve_commands(void *pvParameters) {
  networking_main();

  // After network server is setup, delete this task
  vTaskDelete(NULL);
}

extern "C" {
  void app_main();
}

void app_main() {
  // TODO investigate
  // board_led_operation, board_led_init
  // Onboard LED
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_2, 0);

  {
    bool disable_lights = 0;
    gpio_reset_pin(LIGHTS_DISABLE_PIN);
    gpio_set_direction(LIGHTS_DISABLE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(LIGHTS_DISABLE_PIN, GPIO_FLOATING);
#define LIGHTS_DISABLE_PIN GPIO_NUM_15
    gpio_set_level(LIGHTS_DISABLE_PIN, disable_lights);
    if (disable_lights) {
      ESP_LOGI(TAG, "LIGHTS DISABLED AT 3->5 volt converter\n");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // TODO flash a ACK pattern


  xMutex = xSemaphoreCreateMutex();

  /**
   * Tasks must never end:
   * From https://www.freertos.org/a00125.html
   * 
   * "Tasks are normally implemented as an infinite loop;
   *  the function which implements the task must never attempt to return or exit.
   *  Tasks can, however, delete themselves."
   */

  ESP_LOGI(TAG, "Creating task for receiving commands()\n");
  xTaskCreate(&recieve_commands, "Recieve Commands", /*usStackDepth=*/ 4000, (void*) NULL, /*uxPriority=*/ 6, NULL);

  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "Creating task for home_lights()\n");
  xTaskCreate(&home_lights_run, "home lights", /*usStackDepth=*/ 4000, (void*) NULL, /*uxPriority=*/ 5, NULL);

  size_t i = 0;
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "MAIN LOOP %u\n", ++i);
  }
}