/**
 * OTA Helper - Adapted from simple_ota_example
*/

#include "ota.hpp"

#include <stdio.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_wifi.h"

#include "protocol_examples_common.h"

#include "nvs_flash.h"

TaskHandle_t update_task;

#define HASH_LEN 32
static const char *bind_interface_name = "sta";
static const char *TAG = "OTA";

/**
 * Let's Encrypt issues 90 day certs to me.
 * Retrieved from https://letsencrypt.org/certificates/
 * https://letsencrypt.org/certs/lets-encrypt-r3.pem
 */
// I wonder why I need end
extern const uint8_t lets_encrypt_ca_pem_start[] asm("_binary_lets_encrypt_r3_pem_start");
extern const uint8_t lets_encrypt_ca_pem_end[]   asm("_binary_lets_encrypt_r3_pem_end");

// TODO pass as param into task
static const char *FIRMWARE_UPGRADE_URL = "http://192.168.86.221:8070/hello-world.bin";


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

/**
 * Ideally this would return the response's "Last-Modified" value
 * That's currently impossible because:
 * esp_http_client.h doesn't include http_header.h so you can dereference some fields
 * esp_http_header_get(...) gets from the client->request not client->response
 */
uint64_t ota_binary_size(esp_http_client_config_t *config) {
    esp_http_client_handle_t client = esp_http_client_init(config);
    esp_http_client_open(client, /* write_len= */ 0);

    int header_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200)
        ESP_LOGI(TAG, "Status = %d, content_length = %d | %d",
                status, esp_http_client_get_content_length(client), header_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return header_len;
}

void simple_ota_example_task(void *pvParameter)
{
    TaskHandle_t selfTask = xTaskGetCurrentTaskHandle();
    xTaskNotifyGive(selfTask);

    size_t current_partition_size = 0;


    ESP_ERROR_CHECK(esp_netif_init());
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * More details in "Establishing Wi-Fi or Ethernet Connection" from examples/protocols/README.md
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Can't find netif from interface description");
        abort();
    }
    struct ifreq ifr;
    esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
    ESP_LOGI(TAG, "Bind interface name is %s", ifr.ifr_name);

    while (1) {
        esp_http_client_config_t config = {
            .url = FIRMWARE_UPGRADE_URL,
            .cert_pem = (char *)lets_encrypt_ca_pem_start,
            .event_handler = _http_event_handler,
            .keep_alive_enable = true,
            .if_name = &ifr,
        };

        for (int i = 0; i < 40; i++) {
            vTaskDelay(pdMS_TO_TICKS(25));
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(25));
            gpio_set_level(GPIO_NUM_2, 0);
        }

        size_t test = ota_binary_size(&config);
        if (current_partition_size == 0)
            current_partition_size = test;
        ESP_LOGI(TAG, "OTA status, current: %d  new: %d\n",
                 current_partition_size, test);

        // {
        //     ESP_LOGE(TAG, "Update taking semaphore");
        //     // Wait for my task to have semaphore
        //     ulTaskNotifyTake(/* clearOnExit= */ pdTRUE, portMAX_DELAY);
        //     ESP_LOGE(TAG, "Update got semaphore (waiting 5s)");
        //     // TODO: Make much large in reality
        //     vTaskDelay(pdMS_TO_TICKS(5000));
        //     ESP_LOGE(TAG, "Update released");
        //     xTaskNotifyGive(selfTask);
        // }

        if (test != current_partition_size) {
            gpio_set_level(GPIO_NUM_2, 1);

            // Wait for my task to have semaphore
            ulTaskNotifyTake(/* clearOnExit= */ pdTRUE, portMAX_DELAY);

            esp_err_t ret = esp_https_ota(&config);
            if (ret == ESP_OK) {
                esp_restart();
            } else {
                ESP_LOGE(TAG, "Firmware upgrade failed");
                // XXX: consider if this is right or not.
                current_partition_size = test;
            }
            xTaskNotifyGive(selfTask);
        }

        // TODO: Make much large in reality
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

// static void print_sha256(const uint8_t *image_hash, const char *label)
// {
//     char hash_print[HASH_LEN * 2 + 1];
//     hash_print[HASH_LEN * 2] = 0;
//     for (int i = 0; i < HASH_LEN; ++i) {
//         sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
//     }
//     ESP_LOGI(TAG, "%s %s", label, hash_print);
// }

// static void get_sha256_of_partitions(void)
// {
//     uint8_t sha_256[HASH_LEN] = { 0 };
//     esp_partition_t partition;

//     // get sha256 digest for bootloader
//     partition.address   = ESP_BOOTLOADER_OFFSET;
//     partition.size      = ESP_PARTITION_TABLE_OFFSET;
//     partition.type      = ESP_PARTITION_TYPE_APP;
//     esp_partition_get_sha256(&partition, sha_256);
//     print_sha256(sha_256, "SHA-256 for bootloader: ");

//     // get sha256 digest for running partition
//     esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
//     print_sha256(sha_256, "SHA-256 for current firmware: ");
// }

void setup_OTA_task(void)
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    //get_sha256_of_partitions();

//    ESP_ERROR_CHECK(esp_event_loop_create_default());

    xTaskCreatePinnedToCore(&simple_ota_example_task, "ota_task", 8192, NULL, 9, (TaskHandle_t*) &update_task, 1);
}