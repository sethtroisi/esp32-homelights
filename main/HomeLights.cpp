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

#include <cmath>
#include <cassert>
#include <cstdint>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

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

// Forward definition to avoid recursive includes
//void loadMIDIEffects(short preset);


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
    ESP_LOGI(TAG, "%s\n", key.c_str());
#endif
}

void logKeyValue(string key, string value) {
#if USE_SERIAL
    ESP_LOGI(TAG, "%s %s\n", key.c_str(), value.c_str());
#endif
}

void logValue(string key, float value) {
#if USE_SERIAL
    ESP_LOGI(TAG, "%s %f\n", key.c_str(), value);
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

#define BUTTON_EXT_GPIO GPIO_NUM_4
// "Boot" button
#define BUTTON_INT_GPIO GPIO_NUM_0

static void configure_manual_button(void)
{
    gpio_reset_pin(BUTTON_EXT_GPIO);
    gpio_reset_pin(BUTTON_INT_GPIO);

    gpio_set_direction(BUTTON_EXT_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_INT_GPIO, GPIO_MODE_INPUT);

    // External button is pull down (tied to gnd)
    gpio_set_pull_mode(BUTTON_EXT_GPIO, GPIO_PULLUP_ONLY);
}

static bool check_next_button(void)
{
    // These buttons are both
    bool button_int = !gpio_get_level(BUTTON_INT_GPIO);
    bool button_ext = !gpio_get_level(BUTTON_EXT_GPIO);

    // if (button_int || button_ext)
    //     ESP_LOGI(TAG, "Buttons: %d %d", button_int, button_ext);

    return button_int || button_ext;
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
    enable_converter();
    configure_manual_button();

    for (int i = 0; i < 20; i++) blink_onboard_led(10);

    /**
     * v1 PCB layout is
     * Connectors: [1 2 3]  [4 5 6] [7 8 -]
     * --------------------------------------------
     * pins:   D13, D12, D14, D27, D26, D25, D33, D32
     * strips:           4    123?           7    8
     */

    /**
     * Note FastLED wants to know pins at compile time and array access
     * seems not to be const expr. So I have to do this.
     */

    NUM_LEDS = 150;
    NUM_STRIPS = 7;
    assert(8 <= MAX_NUM_STRIPS);
    assert(300 <= MAX_NUM_LEDS);

    // Dream Willow (This is 8 pins directly after VIN GND)

#define GRID_STRAND_TYPE WS2812B
#define GRID_COLOR_ORDER EOrder::RGB
#define FIBER_LEDS 64
#define TRUNK_LEDS 300

    //FastLED.addLeds<STRAND_TYPE, GPIO_NUM_13, COLOR_ORDER>(__leds, 0 * MAX_NUM_LEDS, 150);
    //FastLED.addLeds<STRAND_TYPE, GPIO_NUM_12, COLOR_ORDER>(__leds, 1 * MAX_NUM_LEDS, 150);

    // V3 PCB is D26, D27, D14, D12, D13, D18, D19, D23

    // Trunk Body (On the VIN/GND side)
    // Pads
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_26, GRID_COLOR_ORDER>(__leds, 0 * MAX_NUM_LEDS, FIBER_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_27, GRID_COLOR_ORDER>(__leds, 1 * MAX_NUM_LEDS, FIBER_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_14, GRID_COLOR_ORDER>(__leds, 2 * MAX_NUM_LEDS, FIBER_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_12, GRID_COLOR_ORDER>(__leds, 3 * MAX_NUM_LEDS, FIBER_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_13, GRID_COLOR_ORDER>(__leds, 4 * MAX_NUM_LEDS, FIBER_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_18, GRID_COLOR_ORDER>(__leds, 5 * MAX_NUM_LEDS, FIBER_LEDS);
    // This needs inverted color order but because of time pressure we just handle it in PostProcess
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_19, GRID_COLOR_ORDER>(__leds2, 0, TRUNK_LEDS);
    FastLED.addLeds<GRID_STRAND_TYPE, GPIO_NUM_23, GRID_COLOR_ORDER>(__leds2, 0, TRUNK_LEDS);

    // LEDs generally go UP the trunk, which is backwards because start of strip is top of tube
    is_reversed = true;

    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(global_brightness);
    FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    //FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    // Start off for a few seconds to connect / disconnect
    delay(2000);

    // Default pattern to run.
    ProcessCommand(DEFAULT_PATTERN);
}

uint32_t fade_stage = 0;
uint32_t pre_fade_brightness = 0;

void hl_loop() {
    const float INVERSE_MICROS = 1e-6;

    // interupts are disabled during FastLed.show() so we have to guess at timing
    uint64_t write_usec_guess = guess_show_timing_usec();

    micros_last = micros_now;
    micros_now = micros(); // 32 bit => overflows every hour!

    if (global_tDelta < 0) global_tDelta = INVERSE_MICROS;

    // Check for manual pattern advance.
    if (next_button_debounced()) {
        blink_onboard_led(50);

        if (disable_ombre == 0) {
            disable_ombre = 1;
        } else {
            //RefreshLastUpdate();
            // -2 => Next pattern (including OMBRE_WAVING_OMBRE)
            loadMIDIEffects(-2);
            // Stay on pattern for a long time
            last_update_t = 0xFFFFFFFF;
        }
    }

    // After 30-50 seconds go to next pattern
    int32_t no_update_millis = millis() - last_update_t;
    const uint32_t fade_start = 80 * 1000;
    const uint32_t fade_down = fade_start + 7 * 1000;
    const uint32_t fade_up   = fade_down + 8 * 1000;
    bool no_recent_touches = (millis() > last_update_t) && (no_update_millis > fade_start);

    if (fade_stage == 0) {
        if (no_recent_touches) {
            // ESP_LOGI(TAG, "Starting fade after %d with brightness = %d", no_update_millis, global_brightness);

            fade_stage = 1;
            // Fade to black, saving old brightness
            pre_fade_brightness = global_brightness;
        }
    } else if (fade_stage == 1) {
        // Linear fade down from fade_start to fade_down
        global_brightness = pre_fade_brightness - ((uint64_t) pre_fade_brightness * (no_update_millis - fade_start)) / (fade_down - fade_start);
        // ESP_LOGI(TAG, "Fade down %d/%d -> %d", no_update_millis, fade_down, global_brightness);
        if (global_brightness == 0 || (no_update_millis > fade_down)) {
            fade_stage = 2;
            loadMIDIEffects(-1);
        }
    } else if (fade_stage == 2) {
        global_brightness = ((uint64_t) pre_fade_brightness * (no_update_millis - fade_down)) / (fade_up - fade_down);
        // ESP_LOGI(TAG, "Fade up %d/%d -> %d", no_update_millis, fade_up, global_brightness);
        if (global_brightness >= pre_fade_brightness || (no_update_millis > fade_up)) {
            global_brightness = pre_fade_brightness;
            fade_stage = 0;
            last_update_t = millis();
        }
    }

    // Main pattern loop.
    {
        global_frames += 1;

        if (current_pattern == METEOR_SHOWER) {
            NUM_LEDS = (TRUNK_LEDS>>1) + FIBER_LEDS;
        } else {
            NUM_LEDS = (TRUNK_LEDS>>1);
        }

        // HACK to make WAVING_OMBRE and OMBRE_WAVING_OMBRE faster
        if (current_pattern == WAVING_OMBRE || current_pattern == OMBRE_WAVING_OMBRE) {
            NUM_STRIPS = 2;
        } else {
            NUM_STRIPS = 7;
        }


        PatternProcessor();
        //PatternPostProcessor();

        // THIS IS THE POST PROCESSOR CODE

        // Set 0th LED to let us know this is working
        //setPixel(0, ColorMap(256 * global_frames, 3));

        // TRUNK IS STRIP 7 -> Index 6
        uint32_t TRUNK_START = 6 * MAX_NUM_LEDS;

        if (current_pattern == WAVING_OMBRE || current_pattern == OMBRE_WAVING_OMBRE) {
            // Copy strip 1 to TRUNK
            // Copy strip 0 to all FIBER strips
            for (uint32_t i = 0; i < (TRUNK_LEDS>>1); i++) {
                __leds[TRUNK_START + i] = __leds[MAX_NUM_LEDS + i];
            }

            for (uint32_t i = 0; i < FIBER_LEDS; i++) {
                // Each branches "extend" the main trunk.
                CRGB color = __leds[i];
                for (uint32_t strip_i = 0; strip_i < 6; strip_i++) {
                    __leds[strip_i * MAX_NUM_LEDS + i] = color;
                }
            }
        }

        if (current_pattern == METEOR_SHOWER) {
            // ONLY ON STRIP 0 SEE meteorShowerStep:454
            // Effect is reversed and starts at 150+64 going back to 0
            // trunks copies it [64,150+64]
            // the branches "extend" the trunk [0, 64]

            // Main trunk gets last 150 leds.
            for (uint32_t i = 0; i < (TRUNK_LEDS>>1); i++) {
                __leds[TRUNK_START + i] = __leds[i + FIBER_LEDS];
            }

            for (uint32_t i = 0; i < FIBER_LEDS; i++) {
                // Each branches "extend" the main trunk (which is "start" of reversed strip).
                CRGB color = __leds[i];
                for (uint32_t strip_i = 0; strip_i < 6; strip_i++) {
                    __leds[strip_i * MAX_NUM_LEDS + i] = color;
                }
            }
        }


        { // Post Processing to fix COLOR order difference between WS2812B and Neopixel (?) Strips
            for (uint32_t i = 0; i < NUM_LEDS; i++) {
                uint32_t j = TRUNK_START + i;
                __leds2[i] = CRGB(__leds[j].g, __leds[j].r, __leds[j].b);
//                __leds2[i] = CRGB(__leds[j].r, __leds[j].g, __leds[j].b);
            }

            for (uint32_t i = NUM_LEDS; i < MAX_NUM_LEDS; i++) {
                __leds2[i] = CRGB::Black;
            }
        }

        { // Post Processing to double up the trunk strips
            for (uint32_t i = 0; i < (TRUNK_LEDS>>1); i++) {
                uint32_t a = 0 * MAX_NUM_LEDS + i;
                uint32_t b = 0 * MAX_NUM_LEDS + (TRUNK_LEDS-1) - i;
                __leds2[b] = __leds2[a];
            }
        }

        FASTLED_safe_show();
    }

    uint64_t micros_after = micros();
    if (micros_after < micros_now) { // overflow happened
        micros_after += (1LL << 32);
    }

    int32_t delta_usec = (micros_after - micros_now);
    if (delta_usec < write_usec_guess) {
        // FastLED disables interupts so micros & millis doesn't work.
        delta_usec += write_usec_guess;
    }

    global_t = micros_now * INVERSE_MICROS;
    // Broken if interupts are disabled and micros isn't updated
    global_tDelta = (micros_now - micros_last) * INVERSE_MICROS;

    if (global_frames % 1000 == 0) {
        ESP_LOGI(TAG, "%d | %llu => Pattern %d (%llu) will pause %u", global_frames, micros_now, current_pattern, micros_after - micros_now, loop_delay);
    }

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
