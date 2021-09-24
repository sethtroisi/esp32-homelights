/**
 * OTA Helper - Adapted from simple_ota_example
*/

#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void setup_and_poll_ota(TaskHandle_t *patternTask);

#ifdef __cplusplus
}
#endif