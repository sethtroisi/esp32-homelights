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

// SN74HCT245 OUTPUT_ENABLE, active_low
#define ENABLE_PIN GPIO_NUM_15

void FASTLED_safe_show() {
  FastLED.show();

  // In theory could be as low as 50us, needed to not clobber next show
  // In practice 300us seems to work nicely
  ets_delay_us(300);
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