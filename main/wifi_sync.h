#pragma once

#include <stdint.h>

// Related to ieee802154

#define PAN_BROADCAST 0xFFFF
#define CHANNEL 19
const uint16_t PAN_ID = 0x4343; // Don't overlap with POI? or maybe do?

void setup_wifi_sync();
void wifi_sync_send_broadcast(uint8_t* data, uint8_t n);
bool wifi_sync_packet_handler(uint8_t* data, uint8_t* data_n);
