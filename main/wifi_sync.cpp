#include "wifi_sync.h"

#include <cstring>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define RADIO_TAG "wifi_sync_esp_now"
#define ESPNOW_CHANNEL 1
#define PACKET_SIZE 5

static QueueHandle_t packet_rx_queue = NULL;

// I get this from flash status
// Less complete is 54:32:04:09:cb:d4
// With LEDS is f0:f5:bd:07:81:0c
//uint8_t broadcastAddress[] = {0x54, 0x32, 0x04, 0x09, 0xCB, 0xD4};
//uint8_t broadcastAddress[] = {0xF0, 0xF5, 0xDB, 0x07, 0x81, 0x0C};
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_now_peer_info_t peerInfo;


void wifi_sync_send_broadcast(uint8_t* data, uint8_t n) {
  esp_err_t result = esp_now_send(broadcastAddress, data, n);
  if (result == ESP_OK) {
    ESP_LOGI(RADIO_TAG, "wifi_sync successful");
  }
  else {
    ESP_LOGE(RADIO_TAG, "wifi_sync error");
  }
}

bool wifi_sync_packet_handler(uint8_t* data, uint8_t* data_n) {
    uint8_t packet[257];
    if (xQueueReceive(packet_rx_queue, &packet, 0) != pdFALSE) {
        //uint8_t packet_length = packet[0] - 1; /* -1 for packet length */

        // TODO comment this out in production
        //debug_print_packet(&packet[1], packet_length);

        memcpy(data, packet, PACKET_SIZE);
        *data_n = PACKET_SIZE;
        return true;
    }
    return false;
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS){
    ESP_LOGI(RADIO_TAG, "OnDataSent successful");
  } else {
    ESP_LOGI(RADIO_TAG, "OnDataSent failure");
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  ESP_EARLY_LOGI(RADIO_TAG, "rx OK, received %d bytes", len);
  BaseType_t task;
  // TODO there's an issue where len is hard to add to incomingData
  // for now we check it's the expected length (4) here and don't queue if it isn't
  // alternatively we could add len to another xQueue but that feels stupid

  if (len == PACKET_SIZE) {
    xQueueSendToBackFromISR(packet_rx_queue, incomingData, &task);
  }
}

void setup_esp_now() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

  ESP_ERROR_CHECK( esp_now_init() );
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_is_peer_exist(broadcastAddress) || esp_now_add_peer(&peerInfo) != ESP_OK) {
    ESP_LOGE(RADIO_TAG, "peer exist or failed to add peer");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}


// SETUP LOGIC -------------------------------------------------------------

void setup_wifi_sync() {
    packet_rx_queue = xQueueCreate(8, 257);
    setup_esp_now();
};
