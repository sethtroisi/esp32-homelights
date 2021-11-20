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

#include "consts.h"
#include "fake_shader.h"

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#include "color_consts.h"

using std::string;

//---------------------------------------------------------------------------||

// All sorts of things about current pattern, number of leds, ...
#include "globals.h"

// Some effect processors that have been extracted
#include "homegrown_effects.h"
#include "adopted_effects.h"

//---------------------------------------------------------------------------||

#define USE_SERIAL  1

static const char *TAG = "HomeLights";

//---------------------------------------------------------------------------||

// Defined in const
// const uint8_t gamma8[256];
// const uint8_t rng[256][256];
// const float sin_lookup[1025];


// Forward declarations
void clearLonger();

// TODO Can these be cleaned up in esp32?
// No idea why these forward defs are needed
// These prevent "CRGB Does not name a type"
//CRGB cm_Wheel(uint16_t WheelPos);
//CRGB cm_Pinks_v1(uint16_t WheelPos);
//CRGB cm_Pinks_v2(uint16_t WheelPos);
// Prevents CRGB' was not declared in this scope
//void ripples_pattern(CRGB ripple_color, bool wander_color);


CRGB ColorMap(uint16_t WheelPos);
uint32_t guessWheel(CRGB guess);
void setStrip(CRGB color);
void setPixel(short pixel, CRGB color);
void setSinglePixel(uint8_t strip_i, short pixel, CRGB color);
void showStrips();


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


float ripples1d( float k, float nth )
{
    float grass_percent = nth * (1.0 / NUM_LEDS);
    float p = 1.0f * grass_percent - 0.5f;

    // Controls how fast ripples move outwards
    const float movement_speed = 0.00213 * global_s;

    // Add some attenuation to p so moves "backwards" some portion of time
    float backwardser = 5.2 * noise2d( vec2( (global_t - 5.41 * k) * 0.28f, (global_t + 2.1 * k) * 0.14f ) );
    p += movement_speed * (global_t + backwardser);

    float foo = 0.5f + 0.57f * (
        0.500f * noise1d(10.0f * p + fast_sin(4.0f * p + global_t)) +
        0.250f * noise1d(17.3f * p + fast_sin(           global_t * 1.7f)) +
        0.125f * noise1d(31.7f * p)
        );
    // 0.5 +- 0.57 * (0.5 + 0.25 + 0.125) = [0.001, 0.999]

    // Attenuate brightness (0 .. 0.5 .. 0.9 => 0 .. 0.125 .. 0.729)
    foo = 1.0f - foo;
    foo = foo * foo * foo * foo;

//    return foo;
    return 0.02f + 0.98f * foo;
//    return 0.25f + 0.75f * foo;
}

float ripples2d( float nth )
{
    float grass_percent = nth * (1.0 / NUM_LEDS);
    float px = 1.0f * grass_percent - 0.5f;
    vec2 p = vec2(px, 0); // -0.5 to 0.5

    //p.y  = 0.07 * global_t;
    p.x += 0.01003 * global_t * global_s; // Controls how fast ripples move outwards

    float foo = 0.5f + 0.57f * (
        0.500f * noise2d(10.0f * p + vec2(fast_sin(4.0f * p.x + global_t), 0)) +
        0.250f * noise2d(17.3f * p + vec2(fast_sin(5.0f * p.y + global_t * 1.7f), 0)) +
        0.125f * noise2d(31.7f * p)
        );
    // 0.5 +- 0.57 * (0.5 + 0.25 + 0.125) = [0.001, 0.999]

    // Attenuate brightness (0 .. 0.5 .. 0.9 => 0 .. 0.25 .. 0.81)
    foo = 1.0f - foo;
    foo = foo * foo;

//    return foo;
    return 0.02f + 0.98f * foo;
//    return 0.25f + 0.75f * foo;
}

void ripples_pattern(CRGB ripple_color, bool wander_color)
{
    // fade colors in and out with perlinish noise
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        if ((active_strips & (1 << strip_i)) == 0)
            continue;

        for (int i = 0; i < NUM_LEDS; i++) {
            if (current_pattern == OMBRE_WAVING_OMBRE) {
                // Move "forward" but wander a little bit
                const float wander = 400 * noise2d( vec2( (global_t + strip_i) * 0.091f, (i + global_t) * 0.0017f ) );
                const uint16_t color_i = wander * (color_delta_mult >> 3);
                color_a_wheel_i = color_i + ((global_i * color_delta_mult ) >> 4);
                color_a = ColorMap(color_a_wheel_i);
            }

            // amount should be 0 to 1
            float amount = ripples1d( strip_i, 234.56 * strip_i + i + 0.5f );

            CRGB pixel_color = ripple_color.scale8((uint8_t) (255 * amount));
            setSinglePixel(strip_i, i, pixel_color);
        }
    }
}


uint32_t guess_show_timing_usec() {
    size_t num_active_strips = ((active_strips & 1) == 1) + ((active_strips & 2) == 2) + ((active_strips & 4) == 4);
    size_t leds = num_active_strips * NUM_LEDS;

    // Both a 1 and 0 take 1.25us (from .85 high + .40 low OR .45 high + .8 low)
    // 1.25us * 24 bits = 30us / led

    return 30 * leds;
}

//--------------------------------------------------------------------------||
// Main command processor

void PatternProcessor() {
    global_frames += 1;

    if (millis() - last_update_t > (2 * 3600 * 1000)) {
        // After 2 hours change to BLACK;
        current_pattern = NONE;
        active_strips = (1 + 2 + 4);
        global_brightness = DEFAULT_BRIGHTNESS;
        clearLonger();
        last_update_t = millis();
    }

    // Do something to guarentee color_a is not blackish for color_a patterns
    if (color_a.r + color_a.g + color_a.b == 0) {
        switch (current_pattern) {
            case LOADING:
            case METEOR_SHOWER:
            case COLLIDER:
            case TWINKLE:
            case SOLID:
            case OMBRE:
                color_a = CRGB( 28, 157, 255 );
                color_a_wheel_i = guessWheel(color_a);
                break;
            case WAVING:
                color_a = ColorMap(120 * 256);
                color_a_wheel_i = 120 * 256;
                break;
            default:
                // No need to update color for none-listed patterns
                ;
        }
    }

    // For patterns which don't change
    switch (current_pattern) {
        case NONE: case SOLID:
        case FLAG_BI: case FLAG_PRIDE: case FLAG_TRANS:
            loop_delay = 100;
            break;
        default:
            ;
            // No need to update color for none-listed patterns
    }

    // loop_delay has to be recalculated here so that speed changes are adjusted

    switch (current_pattern) {
        case NONE:
            global_brightness = DEFAULT_BRIGHTNESS;
            setStrip(CRGB::Black);
            return;

        case LOADING:
            {
                setStrip(CRGB::Black);
                global_i += 1;

                global_j += global_s;
                setPixel( (global_j >> 6) % NUM_LEDS, color_a);

                // global_s handled in position
                loop_delay = 20;
            }
            return;
        case COMET:
            {
                setStrip(CRGB::Black);
                global_i += 1;

                // bounces back and forth in [0, NUM_LEDS - 6] with cycle (2 * NUM_LEDS -12)
                const int DOUBLE_LEN = 2 * NUM_LEDS - 12;
                global_j += global_s;
                int start = (global_j >> 6) % DOUBLE_LEN;

                if (start > (NUM_LEDS - 6))
                    start = DOUBLE_LEN - start;

                for (int j = start; j < start+6 && j < NUM_LEDS; j++) {
                    CRGB halley_color = ColorMap(1700 * j);
                    setPixel(j, halley_color);
                }

                loop_delay = 20;
            }
            return;
        case SNAKE:
            {
                // SNAKE is very similiar to LOADING/COMET but with a faiding tail
                // TODO: global speed & color change to affect fade & snake speed
                global_i += 1;

                const int DOUBLE_LEN = 2 * NUM_LEDS;

                for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
                    int led0 = (global_i-1) + 173 * strip_i + (1 - strip_i) * (global_i-1) / 10;
                    int led1 = global_i + 173 * strip_i + (1 - strip_i) * global_i / 10;

                    for (int start = led0; start < led1; start++) {
                        if (start % NUM_LEDS == 0){
                            snake_colors[strip_i] = ColorMap(512 * global_i);
                        }

                        int led_x = start % DOUBLE_LEN;
                        int led_i = led_x <= NUM_LEDS ? led_x : DOUBLE_LEN - led_x;

                        setSinglePixel(strip_i, led_i, snake_colors[strip_i]);
                    }
                }
                for (auto &led : leds) {
                    led.fadeToBlackBy(color_delta_mult / 256);
                }

                loop_delay = global_s;
            }
            return;


        case METEOR_SHOWER:
            meteorShowerStep();

            loop_delay = 10;    // shorter to have higher FPS => better
            return;
        case COLLIDER:
            colliderStep();

            loop_delay = 10;    // shorter to have higher FPS => better
            return;

        case TWINKLE:
            twinkleStep();

            loop_delay = 10;
            return;


        case SOLID:
            setStrip(color_a);
            return;

        case FLAG_BI:
        case FLAG_PRIDE:
        case FLAG_TRANS:
            {
                int flag_id = (current_pattern == FLAG_PRIDE) +
                          ((current_pattern == FLAG_TRANS) << 1);
                for (int i = 0; i < NUM_LEDS; i++) {
                    int part =  (i * FLAG_PARTS[flag_id]) / NUM_LEDS;
                    setPixel(i, FLAGS[flag_id][part]);
                }
            }
            return;

        case PRIDE_2015:
            pride();
            loop_delay = 1;
            return;

        case TREE:
            {
                global_i += 1;
                global_j += global_s;
                uint64_t shift_mult = global_j / 50;

                uint64_t width = 10;

                for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
                    for (int led = 0; led < NUM_LEDS; led++) {
                        uint64_t temp = (led + shift_mult + width * strip_i) % (2 * width);
                        setSinglePixel(strip_i, led, temp < width ? CRGB::Red : CRGB::Green);
                    }
                }

                loop_delay = 20;
            }
            return;

        case SHOW_CMAP:
            {
                for (int led = 0; led < NUM_LEDS; led++) {
                    setPixel(led, ColorMap(65535 * led / NUM_LEDS));
                }

                loop_delay = 100;
                return;
            }

        case RAINBOW:
        case DOUBLE_RAINBOW:
        case TRIPLE_RAINBOW:
            {
                global_i += 1;
                // (color_delta_mult/64/64) rainbows on the strip
                //      RAINBOW, DOUBEL, TRIPLE = {1/2, 3/2, 5/2} of a colormap showing
                //
                //      65536 * led / (NUM_LEDS-1) * (color_delta_mult / 64/64)
                //      color_delta_mult/16 * led/ (NUM_LEDS-1)
                // THINK OF THE STRIP AS (color_delta_mult/1024) LONG

                // speed => pattern moves global_s% along the strip each second
                //      65536 * global_i / updates_per_second * global_s%/100

                // Starts at some number based on speed (65536 / 50.4 fps / 100 = 13.00)
                global_j += 13 * global_s;

                uint32_t color_mult = color_delta_mult * 16;

                for (int led = 0; led < NUM_LEDS; led++) {
                    uint64_t temp = global_j + (color_mult * led / NUM_LEDS);
                    setPixel(led, ColorMap(temp));
                }

                loop_delay = 19; // Close to 50.4 fps
            }
            return;

        case OMBRE:
            {
                // high loop_delay looks glitchy
                // factor both speed and color_delta into color_a_wheel_i incr

                global_i += 1;
                global_j += global_s * color_delta_mult;
                color_a_wheel_i = global_j >> 10;
                color_a = ColorMap(color_a_wheel_i);
                setStrip(color_a);

                loop_delay = 20;
                return;
            }

        case WAVING_OMBRE:
        case OMBRE_WAVING_OMBRE:
        case WAVING:
            {
                global_i += 1;

                if (current_pattern == WAVING_OMBRE) {
                    // Move "forward" but wander a little bit
                    const float wander = 100 * noise2d( vec2( global_t * 0.091f, global_t * 0.047f ) );
                    const uint16_t wander_i = wander * (color_delta_mult >> 3);

                    color_a_wheel_i = wander_i + ((global_i * color_delta_mult) >> 4);
                    color_a = ColorMap(color_a_wheel_i);
                }

                ripples_pattern(color_a, current_pattern == OMBRE_WAVING_OMBRE);

                loop_delay = 1; // global_s handled in ripples.
                return;
            }

        case RAVE:
            // Choose a random start and end (which can wrap end ends).
            {
                short start = random(NUM_LEDS);
                short end   = random(NUM_LEDS);
                short lower = std::min(start, end);
                short upper = std::max(start, end);
                short wrap  = random(10);

                CRGB wipe = RAVE_MOOD[random(NUM_MOODS)];

                // Wrap is [0, 10)
                       if (wrap <= 2) { // | ... start
                    for (int i = 0; i < start; i++)     setPixel(i, wipe);
                } else if (wrap <= 5) { // start ... end
                    for (int i = lower; i < upper; i++) setPixel(i, wipe);
                } else if (wrap <= 8) { // end ... |
                    for (int i = start; i < NUM_LEDS; i++)  setPixel(i, wipe);
                } else if (wrap <= 9) { // | ... lower   end ... |
                    for (int i = 0; i < NUM_LEDS; i++)
                        if (i < lower || i > upper)     setPixel(i, wipe);
                }

                loop_delay = 60 + 40 * random_float();
                return;
            }

        case RAVE2:
            // Choose a random start and end (which can wrap end ends).
            {
                short middle = random(NUM_LEDS);
                // Should be normal distribution I think
                short size   = (random(10)) + 8;

                short lower = std::max(middle - size/2, 0);
                short upper = std::min(middle + size/2, NUM_LEDS-1);
                CRGB wipe = RAVE_MOOD[random(NUM_MOODS)];
                for (int i = lower; i <= upper; i++)  setPixel(i, wipe);

                // faster when updating fewer leds
                loop_delay = 40 + 2 * size;
                return;
            }

        case LIGHTNING:
        case LIGHTNING_RIPPLES:
            {
                lightning();
                loop_delay = 1;    // shorter to have higher FPS => better
                return;
            }
        case RESTART:
            // Should be handled elsewhere
            logString("GOT RESTART?");
    }
}


uint32_t HexProcessor(string cmd) {
    // NOTE: assumes that the 32 bit value is RRGGBB
    uint32_t color = 0;
    if (cmd.length() == 7) {
        for (int i = 1; i <= 6; i++) {
            int val = 0;
            char hex = cmd[i];
            if (hex >= '0' && hex <= '9') {
                val = hex - '0';
            } else if (hex >= 'A' && hex <= 'F') {
                val = (hex - 'A') + 10;
            } else if (hex >= 'a' && hex <= 'f') {
                val = (hex - 'a') + 10;
            }
            color = (color << 4) + val;
        }
    }
    return color;
}

static
bool startsWith(string word, string prefix) {
    return word.size() >= prefix.size() && word.substr(0, prefix.size()) == prefix;
}

int ProcessCommand(string cmd) {
    logString("Top: " + cmd);
    last_update_t = millis();

    char first = cmd.length() > 0 ? cmd[0] : ' ';

    // Helps synchronize effects
    // Ignore if pattern doesn't change
    if (!(first == '#' || first == '!' || cmd == "BALL" || (first == 'B' && cmd.length() == 2) ||
        startsWith(cmd, "CDELTA") || startsWith(cmd, "SPEED") || cmd == "REVERSE" || cmd == "DEBUG"))
    {
        global_frames = 0;      // frame counter (reset by pattern change)
        global_i = 0;           // counter (may get reset, jump around)
        global_j = 0;           // counter (map get clobbered by other pattern)
        global_rf = 0;        // Random variable
    }

    if (first == '#' && cmd.length() == 7) {
        color_a = HexProcessor(cmd);
        color_a_wheel_i = guessWheel(color_a);
        //color_a = ColorMap(color_a_wheel_i);
        return color_a_wheel_i;

    } else if (cmd == "SOLID") {
        current_pattern = SOLID;
        return 10;

    } else if (cmd == "BLANK") {
        clearLonger();
        current_pattern = NONE;
        return 11;

    } else if (first == '!') {
        int b = atol(cmd.substr(1).c_str());
        if (0 < b && b <= 255) {
            global_brightness = b;
            // Side affect is to set brightness
            clearLonger();
        }
        return 13;

    } else if (cmd == "FLAG_BI") {
        current_pattern = FLAG_BI;
        return 41;
    } else if (cmd == "FLAG_PRIDE") {
        current_pattern = FLAG_PRIDE;
        return 42;
    } else if (cmd == "FLAG_TRANS") {
        current_pattern = FLAG_TRANS;
        return 43;
    } else if (cmd == "PRIDE" or cmd == "PRIDE_2015") {
        current_pattern = PRIDE_2015;
        return 44;

    } else if (cmd == "LOADING") {
        current_pattern = LOADING;
        return 21;
    } else if (cmd == "COMET") {
        current_pattern = COMET;
        return 22;
    } else if (cmd == "SNAKE") {
        global_s = 32;
        color_delta_mult = 32 * 64;
        current_pattern = SNAKE;
        return 26;
    } else if (cmd == "METEOR_SHOWER") {
        meteorSetup();
        current_pattern = METEOR_SHOWER;
        return 23;
    } else if (cmd == "COLLIDER") {
        colliderSetup();
        current_pattern = COLLIDER;
        return 24;
    } else if (cmd == "TWINKLE") {
        twinkleSetup();
        current_pattern = TWINKLE;
        return 25;

    } else if (cmd == "OMBRE") {
        current_pattern = OMBRE;
        return 71;

    } else if (cmd == "WAVING") {
        current_pattern = WAVING;
        return 72;
    } else if (cmd == "WAVING_OMBRE") {
        current_pattern = WAVING_OMBRE;
        return 73;
    } else if (cmd == "OMBRE_WAVING_OMBRE") {
        current_pattern = OMBRE_WAVING_OMBRE;
        return 74;

    } else if (cmd == "TREE") {
        current_pattern = TREE;
        return 39;

    } else if (cmd == "SHOW_CMAP") {
        current_pattern = SHOW_CMAP;
        global_s = 10;
        return 34;

    } else if (cmd == "RAINBOW") {
        current_pattern = RAINBOW;
        color_delta_mult = 32 * 64;
        global_s = 32;
        return 31;
    } else if (cmd == "DRAINBOW") {
        current_pattern = DOUBLE_RAINBOW;
        color_delta_mult = 3 * 32 * 64;
        global_s = 64;
        return 32;
    } else if (cmd == "TRAINBOW") {
        current_pattern = TRIPLE_RAINBOW;
        color_delta_mult = 5 * 32 * 64;
        global_s = 96;
        return 33;

    } else if (cmd == "RAVE") {
        current_pattern = RAVE;
        return 51;
    } else if (cmd == "RAVE2") {
        current_pattern = RAVE2;
        return 52;
    } else if (cmd == "LIGHTNING") {
        lightningSetup();
        current_pattern = LIGHTNING;
        return 53;
    } else if (cmd == "LIGHTNING_RIPPLES") {
        lightningSetup();
        current_pattern = LIGHTNING_RIPPLES;
        return 54;

    } else if (startsWith(cmd, "CDELTA")) {
        int test = atol(cmd.substr(6).c_str());
        if (test > 0 && test <= 255)
            color_delta_mult = test * 64;
        return test;

    } else if (startsWith(cmd, "SPEED")) {
        int test = atol(cmd.substr(5).c_str());
        if (test > 0 && test <= 255)
            global_s = test;
        return test;

    } else if (cmd == "BALL") {
        active_strips = (1 + 2 + 4);
        clearLonger();
        return active_strips;
    } else if (cmd == "B1" || cmd == "B2" || cmd == "B3") {
        active_strips ^= 1 << (cmd[1] - '1');
        clearLonger();
        return active_strips;

    } else if (startsWith(cmd, "CMAP")) {
        int test = atol(cmd.substr(4).c_str());
        if (test >= 0 && test <= 10)
            global_cm = test;
        return test;

    } else if (cmd == "REVERSE") {
        is_reversed = !is_reversed;
        return 301;

    } else if (cmd == "DEBUG") {
        is_fps_debug = !is_fps_debug;
        return 110;

    } else if (cmd == "RESTART") {
        // TODO how to restart ESP32?
        // System.reset();
        return 404;

    } else {
        current_pattern = NONE;
        return 0;
    }

    return 0;
}


//--------------------------------------------------------------------------||


void show_value(int value) {
    // with 154 =>   1111.........     11111.....     1......
    value = abs(value);

    for (int place = 0; place < 5; place++) {
        int digit = value % 10;
        value /= 10;

        for (int i = 0; i < digit; i++)
            setPixel(5 + place * 15 + i, CRGB::Red);
    }
}


//--------------------------------------------------------------------------||


void setStrip(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        setPixel(i, color);
    }
}

void setPixel(short pixel, CRGB color) {
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        if ((active_strips & (1 << strip_i)) == 0)
            continue;

        int pixel_i = is_reversed ? (NUM_LEDS - pixel - 1) : pixel;
        leds[strip_i * NUM_LEDS + pixel_i] = color;
        //strips[strip_i].setPixelColor(pixel_i, color);
    }
}

void setSinglePixel(uint8_t strip_i, short pixel, CRGB color) {
    if (active_strips & (1 << strip_i)) {
        int pixel_i = is_reversed ? (NUM_LEDS - pixel - 1) : pixel;
        leds[strip_i * NUM_LEDS + pixel_i] = color;
        //strips[strip].setPixelColor(pixel_i, color);
    }
}

void FASTLED_safe_show() {
    FastLED.show();

    // In theory could be as low as 50us, needed to not clobber next show
    // In practice 300us seems to work nicely
    ets_delay_us(300);
}


void showStrips() {
    if (active_strips == (1UL << NUM_STRIPS) - 1UL) {
        FASTLED_safe_show();
    } else {
        // TODO this isn't parallel can I just write black to the other strips?
        for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++)
            if ((active_strips & (1 << strip_i)))
                FastLED[strip_i].showLeds(global_brightness);
        ets_delay_us(300);
    }
}

void clearLonger() {
    for(auto c = CLEDController::head(); c != nullptr; c = c->next()) {
        c->clearLeds(300);
    }

    FastLED.setBrightness(global_brightness);
    FastLED.clear();
    FASTLED_safe_show();
}


// Input a value 0 to 65,535 to get a color value.
// The colours are a transition r -> g -> b -> back to r.
CRGB cm_Wheel(uint16_t WheelPos) {

    // Maps WheelPos[0 -> 65536] -> [0, 3 * 255]
    uint8_t up = (((int) 3 * WheelPos) >> 8) & 255;
    uint8_t down = 255 - up;

    // 21845 is last 255
    // 43690 is 2nd last

    if (WheelPos <= 21845) {
        return CRGB(down, 0, up);
    }
    if (WheelPos <= 43690) {
        return CRGB(0, up, down);
    }
    return CRGB(up, down, 0);

}

CRGB cm_Pinks_v1(uint16_t WheelPos) {
    // Add Red to all Blue
    // Take away Blue from all Red

    // Out and back algorithm
    if (WheelPos >= 32768) {
        WheelPos = 65535 - WheelPos;
    }

    // Maps WheelPos[0 -> 65536] -> [0...255, 0...255]
    uint8_t up = (((int) 2 * WheelPos) >> 8) & 255;

    if (WheelPos < 16384) {
        return CRGB(128 + up, 10, 255);
    }
    // 255 - (up - 128) => 127 - up
    return CRGB(255, 10, 127 - up);
}

CRGB cm_Pinks_v2(uint16_t WheelPos) {
    // TODO assert _PuBu_data length
    // TODO Maybe RdPu? BuPu

    // Out and back algorithm
    if (WheelPos >= 32768) {
        WheelPos = 65535 - WheelPos;
    }

    // Seems to break for some reason?

    constexpr uint16_t steps = 4;

    uint32_t temp = WheelPos * steps;
    uint16_t n = temp >> 15;          // High bits
    uint16_t m = (temp & 32767) >> 7; // Low bits

    CRGB a = _RdPu_data[n];
    CRGB b = _RdPu_data[n + 1];
    return blend(a, b, m);
}

/**
 * TODO: FastLed ColorPalette with 16 bits instead of 256
 * Something like above but uses TProgmemPalette16
 */


CRGB ColorMap(uint16_t WheelPos) {
    if (global_cm == 1) {
        return cm_Pinks_v1(WheelPos);
    }
    if (global_cm == 2) {
        return cm_Pinks_v2(WheelPos);
    }

    // Default of 0 is rainbow
    return cm_Wheel(WheelPos);
}

// TODO rename
uint32_t guessWheel(CRGB guess) {
    // Not sure how to guess.

    // This clearly fails on colors like white.

    uint32_t best_i = 0;
    int error = 1 << 30;

    for (int i = 0; i <= 65535; i++) {
        CRGB test = ColorMap(i);
        int error_t = 0;
        for (int c = 0; c < 3; c++) {
            int error_color = guess[c] - test[c];
            error_t += error_color * error_color;
        }

        if (error_t < error) {
            error = error_t;
            best_i = i;
        }
    }
    return best_i;
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

    /**
     * v0 PCB layout was
     * [ D5 ] [ D4 ]
     * [ D3 ] [ D2 ]
     * [ A2 ] [ A3 ]
     * [ A0 ] [ A1 ]
     * 
     * v1 PCB layout is 
     * pins: D12, D12, D14, D27, D26, D25, D33, D32
     * strips: ? ? ?, ? ? ?, ? ? X
     */

    /**
     * Note FastLED wants to know pins at compile time and array access
     * seems not to be const expr. So I have to do this.
     */

    FastLED.addLeds<STRAND_TYPE, DATA_PIN_1, COLOR_ORDER>(leds, NUM_LEDS);
    if (NUM_STRIPS >= 2) FastLED.addLeds<STRAND_TYPE, DATA_PIN_2, COLOR_ORDER>(leds, 1*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 3) FastLED.addLeds<STRAND_TYPE, DATA_PIN_3, COLOR_ORDER>(leds, 2*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 4) FastLED.addLeds<STRAND_TYPE, DATA_PIN_4, COLOR_ORDER>(leds, 3*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 5) FastLED.addLeds<STRAND_TYPE, DATA_PIN_5, COLOR_ORDER>(leds, 4*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 6) FastLED.addLeds<STRAND_TYPE, DATA_PIN_6, COLOR_ORDER>(leds, 5*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 7) FastLED.addLeds<STRAND_TYPE, DATA_PIN_7, COLOR_ORDER>(leds, 6*NUM_LEDS, NUM_LEDS);
    if (NUM_STRIPS >= 8) FastLED.addLeds<STRAND_TYPE, DATA_PIN_8, COLOR_ORDER>(leds, 7*NUM_LEDS, NUM_LEDS);

    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    FastLED.setDither(DEFAULT_BRIGHTNESS < 255);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);

    // wait a tiny bit to clear.
    delay(10);
    clearLonger();
    delay(10);

    // Default pattern to run.
    ProcessCommand(DEFAULT_PATTERN);
}

void hl_loop() {
    const float INVERSE_MICROS = 1e-6;

    // interupts are disabled during FastLed.show() so we have to guess at timing
    uint64_t write_usec_guess = guess_show_timing_usec();

    micros_last = micros_now;
    micros_now = micros(); // 32 bit => overflows every hour!

    if (global_frames % 100 == 0) {
        ESP_LOGI(TAG, "%d | %llu => %d\n", global_frames, micros_now, current_pattern);
    }

    global_t = micros_now * INVERSE_MICROS;
    global_tDelta = (micros_now - micros_last + write_usec_guess) * INVERSE_MICROS;
    if (global_tDelta < 0) global_tDelta = INVERSE_MICROS;

    {
        PatternProcessor();

        // FastLED disables interupts so micros & millis doesn't work.
        FASTLED_safe_show();
    }

    uint64_t micros_after = micros();
    if (micros_after < micros_now) { // overflow happened
        micros_after += (1LL << 32);
    }


    int32_t delta_usec = (micros_after - micros_now) + write_usec_guess;

    int32_t sleep_usec = std::max(0, std::max(1, loop_delay) * 1000 - delta_usec);

    if (is_fps_debug) {
        // TODO look at FastLED::getFPS()
        int fps = 1 / global_tDelta;
        show_value(fps);

        // This means fps measures pattern + 2 shows
        FASTLED_safe_show();
    } else {
        // Note: documentation says not to set long waits with delayMicroseconds
        delayMicroseconds(sleep_usec % 1000);
        delay(sleep_usec / 1000);
    }
}


//--------------------------------------------------------------------------||
