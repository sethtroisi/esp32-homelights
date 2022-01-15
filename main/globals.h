#pragma once

#include "FastLED.h"
FASTLED_USING_NAMESPACE


//---------------------------------------------------------------------------||
//--------------------------------- Strands ---------------------------------//

#define STRAND_TYPE WS2812B
#define COLOR_ORDER GRB

#define MAX_NUM_STRIPS 1
#define MAX_NUM_LEDS 100
// 150
CRGB leds[MAX_NUM_STRIPS * MAX_NUM_LEDS];

int NUM_LEDS = MAX_NUM_LEDS;
int NUM_STRIPS = MAX_NUM_STRIPS;

#define DATA_PIN_1 12
#define DATA_PIN_2 13
#define DATA_PIN_3 14
#define DATA_PIN_4 25
#define DATA_PIN_5 26
#define DATA_PIN_6 27
#define DATA_PIN_7 32
#define DATA_PIN_8 33


// MAX is 255
#define DEFAULT_BRIGHTNESS  64

//---------------------------------------------------------------------------||
//--------------------------------- Patterns --------------------------------//

#define DEFAULT_PATTERN "TWINKLE"

// Add here and twice in HomeLights.ino
enum Pattern {
    NONE,

    LOADING,

    COMET,
    METEOR_SHOWER,
    COLLIDER,
    TWINKLE,
    TREE,
    SNAKE,

    SOLID,
    OMBRE,

    SHOW_CMAP,

    RAINBOW,
    DOUBLE_RAINBOW,
    TRIPLE_RAINBOW,

    WAVING,
    WAVING_OMBRE,
    OMBRE_WAVING_OMBRE,

    RAVE,
    RAVE2,
    LIGHTNING,
    LIGHTNING_RIPPLES,

    FLAG_BI,
    FLAG_PRIDE,
    FLAG_TRANS,

    PRIDE_2015,

    RESTART,
};

// Number of stripes in each flag
const int FLAG_PARTS[] = { 5, 6, 5 };
// Colors of each stripe, top to bottom
const CRGB FLAGS[][6] = {
    { // Bi flag | 2:1:2
        CRGB(214,   0, 162), // OG: 214, 0, 112
        CRGB(214,   0, 162),

   //     CRGB(245, 122, 237), // OG: 155, 79, 150
   //    CRGB(155, 79, 150), // OG: 155, 79, 150
   //    CRGB(56, 12, 53),
       CRGB(121, 0, 173), // Selected by Amanda

        CRGB(  0,  79, 237),
        CRGB(  0,  79, 237), // OG: 0, 56, 150
    },
    { // Pride flag | 6 equal parts
        CRGB(228,   0,   0),
        CRGB(181,  48,   0), // OG: 255, 140,   0
        CRGB(255, 237,   0),
        CRGB(  0, 128,  38),
        CRGB(  0,  77, 255),
        CRGB(117,   0, 135),
    },
    { // Trans flag | 2:1:2
        CRGB(29,  159, 234),
        CRGB(70,   27,  50),
        CRGB(255, 255, 255),
        CRGB(70,   27,  50),
        CRGB(29,  159, 234),
    },
};
const int NUM_FLAGS = sizeof(FLAGS) / sizeof(FLAGS[0]);

const CRGB RAVE_MOOD[] = {
    CRGB::Black,
    CRGB::Red,
    CRGB::Blue,
    CRGB::Green,
    CRGB(148,  0, 211), // Purple
    CRGB(0,  100, 150), // Lighter Blue
};

const int NUM_MOODS = sizeof(RAVE_MOOD) / sizeof(RAVE_MOOD[0]);


const int MAX_TWINKLE = 100;
// Twinkles per seconds (all trips)
#define TWINKLE_RATE    36.1
#define TWINKLE_FADE    1


const int MAX_METEORS = 10;
#define STAR_SIZE     0.054
#define STAR_VELOCITY (75 / 3.2)


// Seconds per Stars (per strand)
#define STAR_RATE     1.05


const int COLLIDER_BALLS = 4;
const int COLLIDER_MAX_SPEED = 5000;
#define   COLLIDER_MIN_SIZE  1.5;


const uint64_t SNAKE_WIDTH = 40;


//---------------------------------------------------------------------------||
//------------------------------- Global State ------------------------------//

int global_brightness = DEFAULT_BRIGHTNESS;

uint32_t ALL_STRIPS = (1 << MAX_NUM_STRIPS) - 1;
uint32_t active_strips = ALL_STRIPS; // bits for blade 1, 2, 3

uint8_t is_reversed = false;
uint8_t is_fps_debug = false;

Pattern current_pattern = NONE;
uint64_t last_update_t = 0;

int loop_delay = 100;

// Config Variables
int global_s = 32;          // speed (sometimes speed = loop_delay, but loop_delay is inverse speed...)
int global_cm = 0;              // global colormap (wheel, roses, ...)
CRGB color_a = CRGB::Red;       // global color used by several patterns.
uint16_t color_a_wheel_i = 0;   // wheel(i) ~= color_a

// Globals managed by loop()
uint64_t micros_now  = 0;   // micros of current frame
uint64_t micros_last = 0;   // Used to avoid fp precision issues with global_tDelta(?)
float global_t = 0;         // Time (monotonically increasing but sometimes resets to zero)
float global_tDelta = 0;    // Delta from last frame.
int global_frames = 0;      // frame counter (reset by pattern change); incremented in PatternProcessor after each frame

// Globals that patterns can clobber
int global_i = 0;           // counter (may get reset, jump around)
uint64_t global_j = 0;           // counter (map get clobbered by other pattern)
float global_rf = 0;        // Random variable




int color_delta_mult = 16 * 256;    // color_a_wheel_i += color_delta_mult

struct meteor {
    bool active;
    uint16_t wheel_i;
    int16_t speed;
    uint8_t length;
    uint8_t sin;
    float   pos_x;

    uint16_t wheel_i_delta;
};

meteor meteors[MAX_NUM_STRIPS][MAX_METEORS >= COLLIDER_BALLS ? MAX_METEORS : COLLIDER_BALLS];

struct twinkle {
    bool active;
    uint16_t wheel_i;
    uint8_t position;
    uint8_t amount;
};

twinkle twinkles[MAX_NUM_STRIPS][MAX_TWINKLE];

struct lighting {
    int8_t remaining_strikes;
    bool is_leader;

    uint16_t brightness;
    uint8_t start_led[MAX_NUM_STRIPS];
    uint8_t end_led[MAX_NUM_STRIPS];

    float start_time;  // relates to global_t
    float on_time;     //           global_t <= on_time  | lightening is on
    float off_time;    // on_time < global_t <= off_time | lightening is off

} lightning_effect;



CRGB snake_colors[MAX_NUM_STRIPS] = {CRGB::Red};
