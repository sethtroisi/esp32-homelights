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
//#include "tweaks.h"


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

// MPR121 stuff
#include "mpr121.h"

MPR121_t cap_sensor_a;
MPR121_t cap_sensor_b;

void capSensorSetup(MPR121_t& cap_sensor, int16_t i2c_addr) {
    uint16_t touchThreshold = 40;
	uint16_t releaseThreshold = 20;

	ESP_LOGI(TAG, "CONFIG_I2C_ADDRESS=0x%X", i2c_addr);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_IRQ_GPIO=%d", CONFIG_IRQ_GPIO);

 	bool ret = MPR121_begin(&cap_sensor, i2c_addr, touchThreshold, releaseThreshold, CONFIG_IRQ_GPIO, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
 	ESP_LOGI(TAG, "MPR121_begin=%d", ret);

	if (ret == false) {
		switch (MPR121_getError(&cap_sensor)) {
			case NO_ERROR:
				ESP_LOGE(TAG, "no error");
				break;
			case ADDRESS_UNKNOWN:
				ESP_LOGE(TAG, "incorrect address");
				break;
			case READBACK_FAIL:
				ESP_LOGE(TAG, "readback failure");
				break;
			case OVERCURRENT_FLAG:
				ESP_LOGE(TAG, "overcurrent on REXT pin");
				break;
			case OUT_OF_RANGE:
				ESP_LOGE(TAG, "electrode out of range");
				break;
			case NOT_INITED:
				ESP_LOGE(TAG, "not initialised");
				break;
			default:
				ESP_LOGE(TAG, "unknown error");
				break;
		}
		while(1) {
			vTaskDelay(1);
		}
	}


#if 0
	MPR121_setTouchThresholdAll(&cap_sensor, 40);	// this is the touch threshold - setting it low makes it more like a proximity trigger, default value is 40 for touch
	MPR121_setReleaseThresholdAll(&cap_sensor, 20);	// this is the release threshold - must ALWAYS be smaller than the touch threshold, default value is 20 for touch
#endif


	MPR121_setFFI(&cap_sensor, FFI_10); // AFE Configuration 1
	MPR121_setSFI(&cap_sensor, SFI_10); // AFE Configuration 2
	MPR121_setGlobalCDT(&cap_sensor, CDT_4US);  // reasonable for larger capacitances
	MPR121_autoSetElectrodesDefault(&cap_sensor, true);	// autoset all electrode settings
}


void flashColorSync(uint32_t color, uint32_t time_ms) {
    // Backup __leds
    memcpy(__leds2, __leds, sizeof(__leds));

    setStrip(color);
    showStrips();

    // Restore __leds
    memcpy(__leds, __leds2, sizeof(__leds));

    delay(time_ms);
}

void SetRippleEffect(uint32_t color, float motion_speed, float density) {
    flashColorSync(color, 100);

    color_a = color;

    // TODO test plumbing of motion_speed & density
 //   RIPPLE_DRIFT_SPEED = motion_speed;
 //   RIPPLE_DENSITY = density;
    ProcessCommand("WAVING");
}

void checkCapSensorPattern() {

	MPR121_updateAll(&cap_sensor_a);
	MPR121_updateAll(&cap_sensor_b);

	for (int i = 0; i < 23; i++) {
        MPR121_t *ptr = (i < 12) ? &cap_sensor_a : &cap_sensor_b;
		if (MPR121_isNewTouch(ptr, i % 12)) {
			ESP_LOGI(TAG, "electrode %d was just touched", i);
		} else if (MPR121_isNewRelease(ptr, i % 12)) {
			ESP_LOGI(TAG, "electrode %d was just released", i);
		}
	}

    // Check total number of buttons pressed
    uint8_t count = 0;
    for (uint i = 0; i < 12; i++) {
        count += MPR121_getTouchData(&cap_sensor_a, i);
        count += MPR121_getTouchData(&cap_sensor_b, i);
    }

    /*
     * Logic is
     *      Any (debounced) press -> flash a frame of its color (synconously and with a copy of leds)

     *      2 matching plates
     *          Flash color (more brightly) and change to that pattern
     *      4 left colors (fire, water, earth, wind)
     *          Rippling "flag" of 4 colors rotating clockwise
     *      4 right colors
     *          Rippling "flag" of 4 colors rotating counter-clockwise
     *      All 8 color plates
     *          ???
     *      All left / right plate?
     *          Ombre Rippling patterns with wind?
     */

    uint32_t last_data = cap_sensor_a.lastTouchData | (((uint32_t) cap_sensor_b.lastTouchData) << 12);
    uint32_t cur_data = cap_sensor_a.touchData | (((uint32_t) cap_sensor_b.touchData) << 12);

    // Find if any extra are pressed
    uint32_t new_data = cur_data - (last_data & cur_data);

    if (new_data) {
        uint32_t flash_color =
            (new_data & 0x0003) ? CRGB::Blue :
                ((new_data & 0x000C) ? CRGB::Red :
                    ((new_data & 0x0030) ? CRGB::Green : CRGB::White));

        flash_color = blend(flash_color, CRGB::Black, 64);
        flashColorSync(flash_color, 100);
    }


    if (count == 2) {
        last_update_t = millis();
        // Pairs
        if (new_data == 0x0003) { /* Water */ SetRippleEffect(CRGB::Blue,  1.5, 1.0); }
        if (new_data == 0x000C) { /* Fire  */ SetRippleEffect(CRGB::Red,   3,   0.4); }
        if (new_data == 0x0030) { /* Earth */ SetRippleEffect(CRGB::Green, 0.5, 1.0); }
        if (new_data == 0x00C0) { /* Air   */ SetRippleEffect(CRGB::White, 2,   0.7); }

        if (new_data == 0x0300) {
            /* Snake, using last color */
            for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++)
                snake_colors[strip_i] = color_a;
            ProcessCommand("SNAKE");
        }
        if (new_data == 0x0C00) {
            /* Twinkle */
            // TODO XXX see if this can interact with past pattern by fading it down slowly?
            ProcessCommand("TWINKLE");
        }
        if (new_data == 0x3000) {
            /* Rainbow */
            ProcessCommand("RAINBOW");
        }

    } else if (new_data == 0b01010101) {
            // TODO only forward
            /* Four Left */
            SetRippleEffect(0, 0.5, 1.0);
    } else if (new_data == 0b10101010) {
            // TODO only backwards
            /* Four Right */
            SetRippleEffect(0, -0.5, 1.0);
    } else if (new_data == 0xFF) {
            /* All Eight */
            SetRippleEffect(0, -0.5, 1.0);
    }
}


//--------------------------------------------------------------------------||


void hl_setup() {
   ESP_LOGI(TAG, "hl setup");
   enable_converter();
   configure_manual_button();

   //capSensorSetup(cap_sensor_a, CONFIG_I2C_ADDRESS);
   //capSensorSetup(cap_sensor_b, CONFIG_I2C_ADDRESS + 1);

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

    NUM_LEDS = 64;
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

    // HACK FOR MOURNING OWL both strips are the "same"
    //FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_5, COLOR_ORDER>(__leds, NUM_LEDS);
    //FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_6, COLOR_ORDER>(__leds, NUM_LEDS);
#define DATA_PIN GPIO_NUM_32
#define CLK_PIN GPIO_NUM_33

    FastLED.addLeds<WS2812B, GPIO_NUM_33, EOrder::RGB>(__leds, NUM_LEDS);

    //FastLED.addLeds<ESPIChipsets::APA102, DATA_PIN, CLK_PIN, EOrder::GRB, DATA_RATE_MHZ(25)>(__leds, NUM_LEDS);

    FastLED.setCorrection(TypicalLEDStrip);
    //FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    //FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    //FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    delay(10);

    // Default pattern to run.
    //ProcessCommand(DEFAULT_PATTERN);

    /*
    uint32_t c = 0;
    while(1) {
        for (int32_t iters = 1000; iters < 10000; iters += 1000) {
//            setStrip(CRGB::Black);

            auto before = micros();

            for (int i = 0; i < iters; i++) {
                int16_t p = i % NUM_LEDS;
//                __leds[p] = test[(i >> 8) & 3]; //ColorMap(c += 16, 0);

//                for (int16_t p : test2) {
//                    __leds[p] = ColorMap(c += 16, 0);
//                }

                //int16_t p = test2[i % (sizeof(test2) / sizeof(test2[0]))];
                __leds[p] = ColorMap((96 * p)  + (c += 8), 0);


                //FastLED[0].showLeds(global_brightness);
                FastLED.show(255);

//                __leds[(i - 5) % NUM_LEDS] = CRGB::Black;
//                delay(1);
//                ets_delay_us(10);
            }
            auto delta = micros() - before;
            ESP_LOGI(TAG, "%u iters took %lu (%lu per) -> FPS %.2f (%u)", iters, delta, delta / iters, 1e6 * iters / delta, c);
        }
    }
    */

    /*
    uint32_t c = 0;
    uint64_t rng = 0;
    while (1) {
        rng = (rng * 134775813 + 1);
        uint32_t prng = rng >> 32;
        for (int i = 0; i < NUM_LEDS; i++) {
            // Want only 1/8th of LEDS on
            bool on = (prng & 0b11100) == (i & 0b11100);
            __leds[i] = !on ? CRGB::Black : ColorMap((8 * 255 * i)  + (c += 2), 0);
        }
        FastLED.show(255);
        delay(250);
    }
    */


    // Load Twinkle Midi
    loadMIDIEffects(0);
}

// Global ish debounce thing


#include "mpr121.h"



void hl_loop() {
    const float INVERSE_MICROS = 1e-6;

    // interupts are disabled during FastLed.show() so we have to guess at timing
    uint64_t write_usec_guess = guess_show_timing_usec();

    micros_last = micros_now;
    micros_now = micros(); // 32 bit => overflows every hour!

    if (global_tDelta < 0) global_tDelta = INVERSE_MICROS;

    //checkCapSensorPattern();

    // Check for manual pattern advance.
    if (next_button_debounced()) {
        blink_onboard_led(50);

        //RefreshLastUpdate();
        // -1 => Next pattern (including blanks)
        loadMIDIEffects(-1);
        last_update_t = millis();
        //last_update_button_t = millis();
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

    if (global_frames % 400 == 0) {
        ESP_LOGI(TAG, "%d | %llu => Pattern %d (%llu)", global_frames, micros_now, current_pattern, micros_after - micros_now);
    }

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
