/*
 * Project HomeLights
 * Description: Lights for Loops and Bloops
 * Author: Seth Troisi
 * Date: 2019-2021
 *
 * v0 Used neopixel.h
 * v1 Using FastLED
 * v2 converted for ESP32
 *
 */

// TODO where is order does this belong
#include "HomeLights.h"

#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "nvs_flash.h"


#include "consts.h"
#include "globals.h"
#include "fake_shader.h"

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#include "color_consts.h"

using std::string;

//---------------------------------------------------------------------------||

// All sorts of things about current pattern, number of leds, ...
#include "globals.h"

#include "tweaks.h"
#include "PatternRunner.h"
#include "wifi_sync.h"

// Forward definition to avoid recursive includes
//void loadNextEffects(short preset);


// Some effect processors that have been extracted
//#include "homegrown_effects.h"
//#include "adopted_effects.h"

//---------------------------------------------------------------------------||

// Build in LED
//#define BLINK_GPIO 2

#define USE_SERIAL  1

static const char *TAG = "HomeLights";


//--------------------------------------------------------------------------||

void logString(string key) {
#if USE_SERIAL
    ESP_LOGI(TAG, "%s", key.c_str());
#endif
}

void logKeyValue(string key, string value) {
#if USE_SERIAL
    ESP_LOGI(TAG, "%s %s", key.c_str(), value.c_str());
#endif
}

void logValue(string key, float value) {
#if USE_SERIAL
    ESP_LOGI(TAG, "%s %f", key.c_str(), value);
#endif
}

//--------------------------------------------------------------------------||


void FASTLED_safe_show() {
    // In theory could be as low as 50us, needed to not clobber next show
    // In practice 300us seems to work nicely
    ets_delay_us(300);

    FastLED.show(global_brightness);

    // Be double safe
    ets_delay_us(300);
}


//--------------------------------------------------------------------------||

// SN74HCT245 OUTPUT_ENABLE, active_low
#define LIGHTS_DISABLE_PIN GPIO_NUM_25
#define ONBOARD_LED_PIN GPIO_NUM_2

static void blink_onboard_led(uint16_t duration_millis) {
    gpio_set_level(ONBOARD_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_millis));
    gpio_set_level(ONBOARD_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(duration_millis));
}

static void enable_converter() {
    // TODO investigate
    // board_led_operation, board_led_init
    // Onboard LED

    gpio_set_direction(ONBOARD_LED_PIN, GPIO_MODE_OUTPUT);

    {
        const bool disable_lights = 0;
        gpio_reset_pin(LIGHTS_DISABLE_PIN);
        gpio_set_direction(LIGHTS_DISABLE_PIN, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode(LIGHTS_DISABLE_PIN, GPIO_FLOATING);
        gpio_set_level(LIGHTS_DISABLE_PIN, disable_lights);
        if (disable_lights) {
            ESP_LOGI(TAG, "LIGHTS DISABLED AT 3->5 volt converter\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// D2 is connected to onboard LED which could be fun (if I pulled it high?)

#define BUTTON_INT_GPIO GPIO_NUM_0

static void configure_manual_button(void)
{
    gpio_reset_pin(BUTTON_INT_GPIO);

    gpio_set_direction(BUTTON_INT_GPIO, GPIO_MODE_INPUT);
}

static bool check_next_button(void)
{
    // These buttons are both
    bool button_int = !gpio_get_level(BUTTON_INT_GPIO);

    // if (button_int || button_ext)
    //     ESP_LOGI(TAG, "Buttons: %d %d", button_int, button_ext);

    return button_int;
}

/**
 * @brief Poll this frequently to see when button_state changes
 *
 * @return true once (and only once) per button press.
 */
static bool next_button_debounced(void)
{
    // Not real MILLIS because of interupt disable in FASTLED
    const uint64_t DEBOUNCE_MILLIS = 10;

    // What the last "debounced" state was
    static bool button_last_state = 0;
    // last button was button_last_state.
    static uint64_t button_state_time = 0;

    bool button_state = check_next_button();

    if (button_state == button_last_state) {
        button_state_time = millis();
    } else {
        if (millis() > button_state_time + DEBOUNCE_MILLIS) {
            ESP_LOGI(TAG, "Buttons: %d @ %llu", button_state, button_state_time);
            button_last_state = button_state;
            if (button_state) {
                return true;
            }
        }
    }
    return false;
}


//--------------------------------------------------------------------------||

void hl_setup() {
    ESP_LOGI(TAG, "hl setup");
    nvs_flash_init(); // Needed for wifi
    enable_converter();
    configure_manual_button();
    setup_wifi_sync();

    for (int i = 0; i < 20; i++) blink_onboard_led(10);

    /**
     * v3 - 2023-08-21 PCB layout is
     * Connectors: [1 2 3]  [4 5 6] [7 8 -]
     * --------------------------------------------
     * pins:   D18, D23, D19, D14, D13, D12, D27, D26
     */

    /**
     * Note FastLED wants to know pins at compile time and array access
     * seems not to be const expr. So I have to do this.
     */

    NUM_LEDS = 300;
    NUM_STRIPS = 1;
    assert(NUM_STRIPS <= MAX_NUM_STRIPS);
    assert(NUM_LEDS <= MAX_NUM_LEDS);

    // Dream Willow (This is 8 pins directly after VIN GND)

#define CLOCK_PIN   GPIO_NUM_18
#define DATA_PIN    GPIO_NUM_23

    gpio_set_direction(CLOCK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DATA_PIN, GPIO_MODE_OUTPUT);

    FastLED.addLeds<SK9822, DATA_PIN, CLOCK_PIN, EOrder::BGR, DATA_RATE_MHZ(12)>(__leds2, NUM_LEDS+1);

    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(global_brightness);
    FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    //FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    // Start off for a few seconds to connect / disconnect
    delay(1500);

    // Default pattern to run.
    ProcessCommand(DEFAULT_PATTERN);
}


uint32_t last_button_t = 0;
uint32_t fade_stage = 0;
uint32_t pre_fade_brightness = 0;

void hl_loop() {
    const float INVERSE_MICROS = 1e-6;

    micros_last = micros_now;
    micros_now = micros(); // 32 bit => overflows every hour!
    uint32_t millis_now = micros_now / 1000l;

    if (global_tDelta < 0) global_tDelta = INVERSE_MICROS;

    // Check for manual pattern advance.
    if (next_button_debounced()) {
        blink_onboard_led(50);

        // Stay on pattern for a long time (if pushed more than one time, one time is just forcing a sync)
        bool stay_awhile = (last_button_t + 5'000) > millis_now;
        last_button_t = millis_now;
        loadNextEffects();

        uint8_t data[] = {
          0,
          (uint8_t) global_last_preset,
          (uint8_t) current_pattern,
          (uint8_t) 0,
            (uint8_t) (stay_awhile ? 0 : 1), // Updated 0ms ago -> sentital
        };
        wifi_sync_send_broadcast(data, sizeof(data));

        if (stay_awhile) {
            last_update_t = millis_now + 3'600'000;
        } else {
            last_update_t = millis_now;
        }
    }

    if (1) { // WIFI disabled while hacking
        const uint32_t INTERVAL_MS_WIFI_NEXT = 25'000'000;
        static uint64_t next_wifi_next       = micros() + INTERVAL_MS_WIFI_NEXT;

        const uint32_t INTERVAL_MS_WIFI_SYNC = 500'000;
        static uint64_t next_wifi_sync       = micros() + INTERVAL_MS_WIFI_SYNC;
        if (micros_now > next_wifi_sync) {
            // ESP_LOGI(TAG, "Syncing Patterns");
            uint8_t data[128];
            uint8_t data_size;
            if (wifi_sync_packet_handler(data, &data_size)) {
                if (data_size == 5) {
                    short delta = (data[3] << 8) + data[4];
                    int current_delta = millis_now - last_update_t;

                    ESP_LOGI(TAG, "Wifi Sync RX {%3u, %3u, %3u, %3u, %3u}",
                        data[0], data[1], data[2], data[3], data[4]);

                    if (global_last_preset == data[1] && current_pattern == data[2]) {
                        ESP_LOGI(TAG, "Sync is same!");
                    } else {
                        ESP_LOGI(TAG, "Sync updating %u/%u to %u/%u!",
                            (uint8_t) global_last_preset, (uint8_t)current_pattern,
                            data[1], data[2]);
                        if (fade_stage > 0) {
                            global_brightness = pre_fade_brightness;
                            fade_stage = 0;
                        }
                        global_last_preset = data[1] - 1;
                        loadNextEffects();
                    }

                    ESP_LOGI(TAG, "Sync delta %u updated to %u", current_delta, delta);
                    if (delta == 0) {
                        last_update_t = millis_now + 3'600'000;
                    } else {
                        last_update_t = millis_now - delta;
                    }
                    // Basically make this less likely to conflict
                    next_wifi_next = micros() + INTERVAL_MS_WIFI_NEXT + 5'000;
                } else {
                    ESP_LOGI(TAG, "Got packet with %u bytes of data???", data_size);
                }
            }
            next_wifi_sync = micros() + INTERVAL_MS_WIFI_SYNC;
        }

        if (micros_now > next_wifi_next && fade_stage == 0) {
            static uint8_t counter = 0;
            int delta = millis_now > last_update_t ? millis_now - last_update_t : 0;
            uint8_t data[] = {
                counter++,
                (uint8_t) global_last_preset,
                (uint8_t) current_pattern,
                (uint8_t) (delta >> 8),
                (uint8_t) (delta & 0xFF),
            };
            wifi_sync_send_broadcast(data, sizeof(data));
            ESP_LOGI(TAG, "Wifi Sync TX {%3u, %3u, %3u, %3u, %3u} (delta: %d)", data[0], data[1], data[2], data[3], data[4], delta);
            next_wifi_next = micros() + INTERVAL_MS_WIFI_NEXT;
        }
    }


    // After 30-50 seconds go to next pattern
    int32_t no_update_millis = millis_now - last_update_t;
    const uint32_t fade_start = 22 * 1000;
    const uint32_t fade_down = fade_start + 4'000;
    const uint32_t fade_up   = fade_down + 2'500;
    bool no_recent_touches = (millis_now > last_update_t) && (no_update_millis > fade_start);

    if (fade_stage == 0) {
        if (no_recent_touches) {
            ESP_LOGI(TAG, "Starting fade after %ld with brightness = %d", no_update_millis, global_brightness);

            fade_stage = 1;
            // Fade to black, saving old brightness
            pre_fade_brightness = global_brightness;
        }
    } else if (fade_stage == 1) {
        // Linear fade down from fade_start to fade_down
        global_brightness = pre_fade_brightness - ((uint64_t) pre_fade_brightness * (no_update_millis - fade_start)) / (fade_down - fade_start);
        if (global_brightness == 0 || (no_update_millis > fade_down)) {
            ESP_LOGI(TAG, "Fade down %ld/%lu -> %d", no_update_millis, fade_down, global_brightness);
            fade_stage = 2;

            if (1) {
                delay(100); // Nice pause at all black
                global_brightness = 0;
                FASTLED_safe_show();
            }

            loadNextEffects();
        }
    } else if (fade_stage == 2) {
        global_brightness = ((uint64_t) pre_fade_brightness * (no_update_millis - fade_down)) / (fade_up - fade_down);
        if (global_brightness >= pre_fade_brightness || (no_update_millis > fade_up)) {
            ESP_LOGI(TAG, "Fade up %ld/%lu -> %d", no_update_millis, fade_up, global_brightness);
            global_brightness = pre_fade_brightness;
            fade_stage = 0;
            last_update_t = millis();
        }
    }

    // Main pattern loop.
    {
        global_frames += 1;

        PatternProcessor();

        // THIS IS THE POST PROCESSOR CODE

        // TODO map about 16 pixels backwards so that 1st led is in lower corner
        if (0) {
            //THIS IS DESTRUCTIVE WHICH BREAKS (IN A FUN WAY SNAKE)
            std::rotate(&__leds[0], &__leds[17], &__leds[NUM_LEDS-1]);
            std::copy_n(__leds, NUM_LEDS, __leds2);
        } else {
            size_t j = 0;
            for (size_t i = 16; i < NUM_LEDS; i++) {
                __leds2[j++] = __leds[i];
            }
            for (size_t i = 0; i < 16; i++) {
                __leds2[j++] = __leds[i];
            }
        }

        __leds2[NUM_LEDS] = CRGB::Black;
        FASTLED_safe_show();
    }

    uint64_t micros_after = micros();
    if (micros_after < micros_now) { // overflow happened
        micros_after += (1LL << 32);
    }

    int32_t delta_usec = (micros_after - micros_now);

    global_t = micros_now * INVERSE_MICROS;
    // Broken if interupts are disabled and micros isn't updated
    global_tDelta = (micros_now - micros_last) * INVERSE_MICROS;

    int32_t sleep_usec = std::max(0l, std::max(1, loop_delay) * 1000l - delta_usec);

    if (global_frames % 2000 == 0) {
        ESP_LOGI(TAG, "%d | %llu => Pattern %d | took %llu will pause %ld for loop_delay %u",
            global_frames, micros_now,
            current_pattern,
            micros_after - micros_now, sleep_usec, loop_delay);
    }

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
