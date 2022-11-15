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
#include "fake_shader.h"

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#include "color_consts.h"

using std::string;

//---------------------------------------------------------------------------||

// All sorts of things about current pattern, number of leds, ...
#include "globals.h"

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
#define LIGHTS_DISABLE_PIN GPIO_NUM_15
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
    for (int i = 0; i < 5; i++) {
        blink_onboard_led(100);
    }

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

static void setup_usb_serial ()
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    // TODO look into uart_get_buffered_data_len

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, /* BUFFER SIZE */ 512, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0,
        /* TX pin */ 1, /* RX pin */ 3,
        /* RTS pin */ UART_PIN_NO_CHANGE, // technically 16
        /* CTS pin */ UART_PIN_NO_CHANGE)); // technically 17

}

uint8_t read_serial_byte() {
    // Configure a temporary buffer for the incoming data
    uint8_t data = 0;

    // Read data from the UART
    int len = uart_read_bytes(UART_NUM_0, &data, 1, 20 / portTICK_PERIOD_MS);
    if (len) {
        //ESP_LOGI(TAG, "Recv %d", data);
        return data;
    } else {
        return 0;
    }
}

//--------------------------------------------------------------------------||


void hl_setup() {
    /* TODO find an ESP32 replacement
    DeviceNameHelperEEPROM::instance().setup(EEPROM_OFFSET_DEVICE_NAME);
    for (int i = 0; i < 50 && !DeviceNameHelperEEPROM::instance().hasName(); i++) {
        DeviceNameHelperEEPROM::instance().loop();
        delay(1000);
    }

    string name = DeviceNameHelperEEPROM::instance().getName();
    */

   enable_converter();
   setup_usb_serial();
   configure_manual_button();

    /**
     * v0 PCB layout was
     * [ D5 ] [ D4 ]
     * [ D3 ] [ D2 ]
     * [ A2 ] [ A3 ]
     * [ A0 ] [ A1 ]
     *
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

    NUM_LEDS = 136;
    NUM_STRIPS = 1;
    assert(NUM_STRIPS <= MAX_NUM_STRIPS);

#define DATA_PIN_CONN_1 32
#define DATA_PIN_CONN_2 33
#define DATA_PIN_CONN_3 25
#define DATA_PIN_CONN_4 26
#define DATA_PIN_CONN_5 27
#define DATA_PIN_CONN_6 14
#define DATA_PIN_CONN_7 12
#define DATA_PIN_CONN_8 13

    /**
     * TODO should be nice to write a few black pixel past the end of each LED but will certainly break
     * this or other code
     */

    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_4, COLOR_ORDER>(__leds, NUM_LEDS);
    // HACK FOR MOURNING OWL both strips are the "same"
    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_5, COLOR_ORDER>(__leds, NUM_LEDS);
    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_6, COLOR_ORDER>(__leds, NUM_LEDS);

    // if (NUM_STRIPS >= 2) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_5, COLOR_ORDER>(__leds, 1*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 3) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_6, COLOR_ORDER>(__leds, 2*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 4) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_1, COLOR_ORDER>(__leds, 3*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 5) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_2, COLOR_ORDER>(__leds, 4*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 6) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_3, COLOR_ORDER>(__leds, 5*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 7) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_7, COLOR_ORDER>(__leds, 6*NUM_LEDS, NUM_LEDS);
    // if (NUM_STRIPS >= 8) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_8, COLOR_ORDER>(__leds, 7*NUM_LEDS, NUM_LEDS);

    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    delay(10);

    // Default pattern to run.
    //ProcessCommand(DEFAULT_PATTERN);

    // Load Twinkle Midi
    loadMIDIEffects(0);
}

// Global ish debounce thing

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
        last_update_button_t = millis();
    }

    // Main pattern loop.
    {
        CheckAndProcessMIDI();
        PatternProcessor();
        //PatternPostProcessor();

        // // Set 0th LED to let us know this is working
        // setPixel(0, ColorMap(256 * global_frames, 3));

        // // Set 1st LED to let us see MIDI events being processed
        // setPixel(1, ColorMap(256 * global_MIDI_count, 3));

        // Turn of the "extra" LEDs. This keeps them from occasionally becoming a color
        for (uint32_t i = NUM_LEDS; i < MAX_NUM_LEDS; i++)
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

    if (global_frames % 200 == 0) {
        ESP_LOGI(TAG, "%d | %llu => Pattern %d (%llu)", global_frames, micros_now, current_pattern, micros_after - micros_now);
    }

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Removed is_fps_debug handling.

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
