/* Handles HomeLights Networking

   Using a lot of example code from http_server/simple

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <string>

#include "driver/gpio.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "protocol_examples_common.h"


// HomeLights.h
int ProcessCommand(std::string cmd);
void clearLonger();

// main.cpp
extern SemaphoreHandle_t xMutex;


static const char *TAG = "HL_Networking";

/**
 * req->uri
 *      has request url e.g. "/lights"
 * 
 * len = httpd_req_get_hdr_value_len(req, header_name) + 1;
 *      get request header value

 * len = httpd_query_key_value(query, key, val, max_size);
 *      get query arg value
 * 
 * ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf))));
 *      reads the post data
 */

static esp_err_t lights_get_handler(httpd_req_t *req)
{
    char buf[100] = {};
    char cmd[20] = "ERROR";

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf) -1) == ESP_OK) {
        ESP_LOGI(TAG, "query str: %s\n", buf);
        esp_err_t err = httpd_query_key_value(buf, "cmd", buf, sizeof(buf) - 1);
        if (err == ESP_OK) {
            strncpy(cmd, buf, sizeof(cmd));
            cmd[sizeof(cmd)-1] = '\0';
        } else {
            ESP_LOGI(TAG, "no CMD arg: %s\n", req->uri);
        }
    } else {
        ESP_LOGI(TAG, "no query str: %s\n", req->uri);
    }

    int response_code = 404;

    if (strlen(cmd) && strcmp(cmd, "ERROR") != 0) {
        ESP_LOGI(TAG, "lights CMD: %s\n", cmd);

        // Probably should lock till other thread finishes a step
        xSemaphoreTake( xMutex, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));
        response_code = ProcessCommand(cmd);
        vTaskDelay(pdMS_TO_TICKS(100));
        clearLonger();
        vTaskDelay(pdMS_TO_TICKS(100));
        xSemaphoreGive( xMutex );
    }

    /* Send response with body set as the response code */
    snprintf(buf, sizeof(buf), "%s -> %d\n", cmd, response_code);
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static const httpd_uri_t lights = {
    .uri       = "/lights",
    .method    = HTTP_GET,
    .handler   = lights_get_handler,
    .user_ctx  = NULL
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &lights);
        httpd_register_uri_handler(server, &echo);
        ESP_LOGI(TAG, "Registered URI handlers");
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void networking_main(void)
{
    static httpd_handle_t server = NULL;

    // TODO this was in examples/http_simple is/what is it needed for?
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    // TODO this was in examples/http_simple is/what is it needed for?
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

    /* Start the server for the first time */
    server = start_webserver();
    ESP_LOGI(TAG, "Server Started");
}
