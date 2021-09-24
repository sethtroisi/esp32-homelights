/**
 * OTA Helper - Adapted from simple_ota_example
*/

#pragma once

#ifndef OTA_H_GUARD
#define OTA_H_GUARD

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char OTA_TASK_NAME[] = "ota_task";

void setup_OTA_task(void);


// #ifdef __cplusplus
// }
// #endif

#endif  // OTA_H_GUARD
