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
void loadMIDIEffects(uint8_t preset);


// Some effect processors that have been extracted
//#include "homegrown_effects.h"
//#include "adopted_effects.h"

//---------------------------------------------------------------------------||

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


void setup_usb_serial ()
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


void hl_setup() {
    /* TODO find an ESP32 replacement
    DeviceNameHelperEEPROM::instance().setup(EEPROM_OFFSET_DEVICE_NAME);
    for (int i = 0; i < 50 && !DeviceNameHelperEEPROM::instance().hasName(); i++) {
        DeviceNameHelperEEPROM::instance().loop();
        delay(1000);
    }

    string name = DeviceNameHelperEEPROM::instance().getName();
    */

   setup_usb_serial();

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

    NUM_LEDS = 75;
    NUM_STRIPS = 3;
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
    if (NUM_STRIPS >= 2) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_5, COLOR_ORDER>(__leds, 1*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 3) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_6, COLOR_ORDER>(__leds, 2*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 4) FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_1, COLOR_ORDER>(__leds, 3*NUM_LEDS, NUM_LEDS);
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
    ProcessCommand(DEFAULT_PATTERN);

    loadMIDIEffects(0);
}

void hl_loop() {
    const float INVERSE_MICROS = 1e-6;

    // interupts are disabled during FastLed.show() so we have to guess at timing
    uint64_t write_usec_guess = guess_show_timing_usec();

    micros_last = micros_now;
    micros_now = micros(); // 32 bit => overflows every hour!

    if (global_tDelta < 0) global_tDelta = INVERSE_MICROS;
    {
        CheckAndProcessMIDI();
        PatternProcessor();
        PatternPostProcessor();

        // Set 0th LED to let us know this is working
        setPixel(0, ColorMap(256 * global_frames, 3));

        // Set 1st LED to let us see MIDI events being processed
        setPixel(1, ColorMap(256 * global_MIDI_count, 3));

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
        ESP_LOGI(TAG, "%d | %llu => Pattern %d (%llu)\n", global_frames, micros_now, current_pattern, micros_after - micros_now);
    }

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Removed is_fps_debug handling.

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
