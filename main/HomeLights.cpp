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

#include "HomeLights.h"

#include <cmath>
#include <cassert>
#include <cstdint>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "consts.h"
#include "fake_shader.h"

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#include "color_consts.h"

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

    FastLED.show();

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

void flashColorSync(CRGB color, uint32_t time_ms) {
    // Backup __leds
    memcpy(__leds2, __leds, sizeof(__leds));

    if (color.r + color.g + color.b > 0) // (color != CRGB::Black)
        setStrip(color);
    else {
        // Rainbow pattern via CRGB::Black
        uint32_t color_mult = color_delta_mult * 16;

        for (int led = 0; led < NUM_LEDS; led++) {
            uint64_t temp = global_j + (color_mult * led / NUM_LEDS);
            setPixel(led, ColorMap(temp, 0));
        }
    }

    showStrips();

    // Restore __leds
    memcpy(__leds, __leds2, sizeof(__leds));

    delay(time_ms);
}

void SetRippleEffect(Pattern pattern, CRGB color, float motion_speed) {
    ESP_LOGI(TAG, "New effect %d with r=%2d, g=%2d, b=%2d | speed=%.2f",
        pattern, color.r, color.g, color.b, motion_speed);

    // Not needed because will have just pressed a new button and got flash from that?
    //if (color != color_a && color != CRGB::Black) {
    //    flashColorSync(color, 150);
    //}

    color_a = color;

    RIPPLE_DRIFT_SPEED = motion_speed;

    // None of these require setting other params so we can directly set current_pattern
    current_pattern = pattern;
}

//--------------------------------------------------------------------------||

bool cap_init_success = false;

void hl_setup() {
    ESP_LOGI(TAG, "hl setup");
    enable_converter();
    configure_manual_button();

    for (int i = 0; i < 20; i++) blink_onboard_led(10);

    /**
     * v3 - 2023-08-21 PCB layout is
     * Connectors: [1 2 3]  [4 5 6] [7 8 -]
     * --------------------------------------------
     * pins:   D18, D23, D19, D14, D13, D12, D27, D26
     *
     * v3 - 2023-08-21 LARGER PCB layout is
     * Connectors: [1 2 3]  [4 5 6] [7 8 -]
     * --------------------------------------------
     * pins:   D26, D27, D14, D12, D13, D18, D19, D23
     *
     */

    /**
     * Note FastLED wants to know pins at compile time and array access
     * seems not to be const expr. So I have to do this.
     */

    NUM_LEDS = 138;
    NUM_STRIPS = 2;
    assert(NUM_STRIPS <= MAX_NUM_STRIPS);

    // Small Board
    FastLED.addLeds<STRAND_TYPE, 18, COLOR_ORDER>(__leds, NUM_LEDS + 3);
    FastLED.addLeds<STRAND_TYPE, 23, COLOR_ORDER>(__leds, NUM_LEDS + 3);
    // large Board
    //FastLED.addLeds<STRAND_TYPE, 26, COLOR_ORDER>(__leds, NUM_LEDS + 3);
    //FastLED.addLeds<STRAND_TYPE, 27, COLOR_ORDER>(__leds, NUM_LEDS + 3);

    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(global_brightness);
    //FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    //FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    delay(10);

    // Default pattern to run.
    ProcessCommand(DEFAULT_PATTERN);
}

// Terrible globals for fade down and up
uint64_t last_human_input_t = 0;
uint64_t last_change_t = 0;
uint8_t fade_stage = 0; // 0 nothing, 1 down, 2 up
uint8_t pre_fade_brightness = 0;

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

        //RefreshLastUpdate();
        // -1 => Next pattern (including blanks)
        loadMIDIEffects(-1);
        last_human_input_t = millis();
        last_change_t = millis();
    }

    // After 30-50 seconds go back to DEFAULT pattern
    uint32_t update_millis_a = millis() - last_human_input_t;
    int32_t no_update_millis = millis() - last_change_t;
    const uint32_t fade_start = 20 * 1000;
    const uint32_t fade_down = fade_start + 10 * 1000;
    const uint32_t fade_up   = fade_down + 7 * 1000;
    bool no_recent_touches = (fade_start < no_update_millis) && ((last_human_input_t == 0) || (update_millis_a > 120 * 1000));


    if (fade_stage == 0) {
        if (no_recent_touches) {
            ESP_LOGI(TAG, "Starting fade after %ld with brightness = %d", no_update_millis, global_brightness);

            fade_stage = 1;
            // Fade to black, saving old brightness
            pre_fade_brightness = global_brightness;
        }
    } else {
        if (no_update_millis < 1000) {
            ESP_LOGI(TAG, "Fade up recent press %ld", no_update_millis);
            // Start bring up immediately.
            fade_stage = 2;
            global_brightness = pre_fade_brightness;
        }
    }

    if (fade_stage == 1) {
        // Linear fade down from fade_start to fade_down
        global_brightness = pre_fade_brightness - ((uint64_t) pre_fade_brightness * (no_update_millis - fade_start)) / (fade_down - fade_start);
//        ESP_LOGI(TAG, "Fade up %d/%d -> %d", no_update_millis, fade_down, global_brightness);
        if (global_brightness == 0 || (no_update_millis > fade_down)) {
            fade_stage = 2;
            global_cm = 0;
            loadMIDIEffects(-1);
        }
    } else if (fade_stage == 2) {
        global_brightness = ((uint64_t) pre_fade_brightness * (no_update_millis - fade_down)) / (fade_up - fade_down);
//        ESP_LOGI(TAG, "Fade up %d/%d -> %d", no_update_millis, fade_up, global_brightness);
        if (global_brightness >= pre_fade_brightness || (no_update_millis > fade_up)) {
            global_brightness = pre_fade_brightness;
            fade_stage = 0;
            last_change_t = millis();
        }
    }

    // Main pattern loop.
    {
        PatternProcessor();
        //PatternPostProcessor();

        // // Set 0th LED to let us know this is working
        // setPixel(0, ColorMap(256 * global_frames, 3));

        // // Set 1st LED to let us see MIDI events being processed
        // setPixel(1, ColorMap(256 * global_MIDI_count, 3));

        // Turn of the "extra" LEDs. This keeps them from occasionally becoming a color
        for (uint32_t i = NUM_LEDS; i < MAX_NUM_LEDS; i++)
            // Can break if is_reversed
            setPixel(i, CRGB::Black);

        showStrips();
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
        ESP_LOGI(TAG, "%d | %llu => Pattern %d (%llu)", global_frames, micros_now, current_pattern, micros_after - micros_now);
    }

    int32_t sleep_usec = std::max<long int>(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
