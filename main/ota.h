/**
 * OTA Helper - Adapted from simple_ota_example
*/

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

static const char OTA_TASK_NAME[] = "ota_task";

void setup_OTA_task(void);

#ifdef __cplusplus
}
#endif