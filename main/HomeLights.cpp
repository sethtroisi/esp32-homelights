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

#define CAP_SENSOR_CODE 0

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

bool capSensorSetup(MPR121_t& cap_sensor, int16_t i2c_addr) {
    const uint16_t touchThreshold = 40;
	const uint16_t releaseThreshold = 20;

    static const char *TAG = "MPR121";

	ESP_LOGI(TAG, "CONFIG_I2C_ADDRESS=0x%X", i2c_addr);
	ESP_LOGI(TAG, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
	ESP_LOGI(TAG, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
	ESP_LOGI(TAG, "CONFIG_IRQ_GPIO=%d", CONFIG_IRQ_GPIO);

 	bool success = MPR121_begin(&cap_sensor, i2c_addr, touchThreshold, releaseThreshold, CONFIG_IRQ_GPIO, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
 	ESP_LOGI(TAG, "MPR121_begin=%d", success);


    if (success) {
        MPR121_setFFI(&cap_sensor, FFI_10); // AFE Configuration 1
        MPR121_setSFI(&cap_sensor, SFI_10); // AFE Configuration 2
        MPR121_setGlobalCDT(&cap_sensor, CDT_4US);  // reasonable for larger capacitances
        MPR121_autoSetElectrodesDefault(&cap_sensor, true);	// autoset all electrode settings
    } else {
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
	}

#if 0
	MPR121_setTouchThresholdAll(&cap_sensor, 40);	// this is the touch threshold - setting it low makes it more like a proximity trigger, default value is 40 for touch
	MPR121_setReleaseThresholdAll(&cap_sensor, 20);	// this is the release threshold - must ALWAYS be smaller than the touch threshold, default value is 20 for touch
#endif

    return success;
}



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

#if CAP_SENSOR_CODE
bool checkCapSensorPattern() {
	MPR121_updateAll(&cap_sensor_a);
	MPR121_updateAll(&cap_sensor_b);

    const uint16_t NUM_SENSORS = 24;
    const uint16_t SENSORS_PER = 12;

	for (int i = 0; i < NUM_SENSORS; i++) {
        MPR121_t *ptr = (i < SENSORS_PER) ? &cap_sensor_a : &cap_sensor_b;
        int s = i % SENSORS_PER;
		if (MPR121_isNewTouch(ptr, s)) {
			ESP_LOGI(TAG, "electrode %d(%d) was just touched", s, i);
		} else if (MPR121_isNewRelease(ptr, s)) {
			ESP_LOGI(TAG, "electrode %d(%d) was just released", s, i);
		}
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

    uint32_t last_data = cap_sensor_a.lastTouchData | (((uint32_t) cap_sensor_b.lastTouchData) << SENSORS_PER);
    uint32_t cur_data = cap_sensor_a.touchData | (((uint32_t) cap_sensor_b.touchData) << SENSORS_PER);

    // Don't do anything if current = last
    if (last_data == cur_data) {
        return false;
    }

    // Check total number of buttons pressed
    uint8_t count = 0;
    for (uint i = 0; i < NUM_SENSORS; i++) {
        count += (cur_data >> i) & 1;
    }

    {
        // Find if any extra are pressed
        uint32_t new_data = cur_data - (last_data & cur_data);
        if (new_data) {
            uint8_t which = new_data | (new_data >> SENSORS_PER);
            CRGB flash_color =
                (which & 0b0001) ? ELEMENT_COLORS[0] :
                    (which & 0b0010) ? ELEMENT_COLORS[1] :
                        (which & 0b0100) ? ELEMENT_COLORS[2] :
                            (which & 0b1000) ? ELEMENT_COLORS[3] :
                                (which & 0b10000) ? CRGB::Black : CRGB::Purple;

            ESP_LOGI(TAG, "new_data: %x -> CRGB(%2d,%2d,%2d)", new_data, flash_color.r, flash_color.g, flash_color.b);
            flash_color = blend(flash_color, CRGB::Black, 128);
            flashColorSync(flash_color, 150);
        }
    }

    uint8_t sensor_pairs = cur_data & (cur_data >> SENSORS_PER);
    if (count > 0 && cur_data != last_data) {
        ESP_LOGI(TAG, "cur=0x%x count=%d | sensor_pairs=0x%x", cur_data, count, sensor_pairs);
    }

    if (count == 2) {
        last_update_t = millis();
        // Pairs
        if (sensor_pairs == 0b00000001) {
             /* Water */
             //SetRippleEffect(WAVING, ELEMENT_COLORS[0], 1.5,    1.0);
            global_cm = 0;
            is_reversed = true;
             ProcessCommand("OCEAN_WAVES");
        }
        if (sensor_pairs == 0b00000010) {
            global_cm = 4; // RedOrange
            is_reversed = false;
            /* Fire  */
            // SetRippleEffect(WAVING_OMBRE,       ELEMENT_COLORS[1], 0.002);
            ProcessCommand("DRAINBOW");
        } else if (sensor_pairs == 0b00000100) {
            global_cm = 5;
            is_reversed = false;
            /* Earth */ SetRippleEffect(OMBRE_WAVING_OMBRE, ELEMENT_COLORS[2], 0.0015);
        } else if (sensor_pairs == 0b00001000) {
            global_cm = 0;
            is_reversed = false;
            /* Air   */ SetRippleEffect(WAVING,             ELEMENT_COLORS[3], 0.005);

        } else if (sensor_pairs == 0b00010000) {
            /* Rainbow */
            global_cm = 0;
            is_reversed = false;
            ProcessCommand("RAINBOW");

        } else if (sensor_pairs == 0b00100000) {
            /* Snake, starting from opposite last color */
            global_cm = 0;
            is_reversed = false;

            color_a_wheel_i = guessWheel(color_a) + 128 * 256;
            for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++)
                snake_colors[strip_i] = ColorMap(color_a_wheel_i);

            ProcessCommand("SNAKE");

        } else if (sensor_pairs == 0b01000000) {
            /* Sparkles / Twinkle */
            global_cm = 0;
            is_reversed = false;
            ProcessCommand("TWINKLE");
        }

    } else if (cur_data == 0xF) {
            // TODO only forward
            /* Four Left */
            global_cm = 0;
            is_reversed = false;
            SetRippleEffect(WAVING_SEGMENTS_1, CRGB::Black, 0.004);
    } else if (cur_data == (0xF << SENSORS_PER)) {
            // TODO only backwards
            /* Four Right */
            global_cm = 0;
            is_reversed = false;
            SetRippleEffect(WAVING_SEGMENTS_1, CRGB::Black, -0.004);
    } else if (sensor_pairs == 0b00001111) {
            /* All Eight */
            global_cm = 0;
            is_reversed = false;
            SetRippleEffect(WAVING_SEGMENTS_2, CRGB::Black, 0.001);

    } else if (sensor_pairs == 0b1111111) {
            /* All Eight */
            global_cm = 0;
            is_reversed = false;
            ProcessCommand("RAVE");
    }


    if (count > 0 && cur_data != last_data) {
        ESP_LOGI(TAG, "At end global_cm=%d", global_cm);
    }

    // Some human input
    return true;
}
#endif // CAP_SENSOR_CODE

//--------------------------------------------------------------------------||

bool cap_init_success = false;

void hl_setup() {
    ESP_LOGI(TAG, "hl setup");
    enable_converter();
    configure_manual_button();

#if CAP_SENSOR_CODE
    cap_init_success = (
        capSensorSetup(cap_sensor_a, CONFIG_I2C_ADDRESS + 1) &&
        capSensorSetup(cap_sensor_b, CONFIG_I2C_ADDRESS)
    );

    if (!cap_init_success) {
        ESP_LOGI(TAG, "bad init");
        // Blink LEDs as error
        for (int i = 0; i < 5; i++) blink_onboard_led(200);
    }
#endif // CAP_SENSOR_CODE

    for (int i = 0; i < 20; i++) blink_onboard_led(10);

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

    NUM_LEDS = 138;
    NUM_STRIPS = 2;
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
    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_4, COLOR_ORDER>(__leds, NUM_LEDS + 3);
    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_5, COLOR_ORDER>(__leds, NUM_LEDS + 3);
    FastLED.addLeds<STRAND_TYPE, DATA_PIN_CONN_6, COLOR_ORDER>(__leds, NUM_LEDS + 3);

    // Dream Willow
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_12, COLOR_ORDER>(__leds, 0 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_14, COLOR_ORDER>(__leds, 1 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_27, COLOR_ORDER>(__leds, 2 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_26, COLOR_ORDER>(__leds, 3 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_25, COLOR_ORDER>(__leds, 4 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_33, COLOR_ORDER>(__leds, 5 * MAX_NUM_LEDS, NUM_LEDS);
    // FastLED.addLeds<STRAND_TYPE, GPIO_NUM_32, COLOR_ORDER>(__leds, 6 * MAX_NUM_LEDS, NUM_LEDS);

//#define DATA_PIN GPIO_NUM_32
//#define CLK_PIN GPIO_NUM_33
    //FastLED.addLeds<ESPIChipsets::APA102, DATA_PIN, CLK_PIN, EOrder::GRB, DATA_RATE_MHZ(25)>(__leds, NUM_LEDS);

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

#if CAP_SENSOR_CODE
    if (cap_init_success && checkCapSensorPattern()) {
        last_human_input_t = millis();
    }
#endif

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
            ESP_LOGI(TAG, "Starting fade after %d with brightness = %d", no_update_millis, global_brightness);

            fade_stage = 1;
            // Fade to black, saving old brightness
            pre_fade_brightness = global_brightness;
        }
    } else {
        if (no_update_millis < 1000) {
            ESP_LOGI(TAG, "Fade up recent press %d", no_update_millis);
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

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    // Note: documentation says not to set long waits with delayMicroseconds
    delayMicroseconds(sleep_usec % 1000);
    delay(sleep_usec / 1000);
}


//--------------------------------------------------------------------------||
