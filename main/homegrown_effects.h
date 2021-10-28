#pragma once

//---------------------------------------------------------------------------||
//--------------------------- Home grown effects ----------------------------//

#include <cmath>

#include "globals.h"

// Forward definitions
CRGB ColorMap(uint16_t WheelPos);
uint32_t guessWheel(CRGB guess);
void setStrip(CRGB color);
void setPixel(short pixel, CRGB color);
void setSinglePixel(uint8_t strip_i, short pixel, CRGB color);


//---------------------------------------------------------------------------||

int random(int max) {
    // TODO check if esp32 supports this
    return random() % max;
}

// TODO check for better definition
int constrain(int a, int min, int max) {
    return a <= min ? min : (a >= max ? max : a); 
}

float random_float() {
    return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}

// TODO see if this can be replaced by sin16_avr
static float fast_sin(float x) {
    //return sin(x);

    // x = fmod(x, 2*M_PI) * 1024/(2*M_PI);
    x = fmod(x, 6.2831853f) * 162.97466f;
    int i = x;
    float frac = x - i;

    float sin_prev  = sin_lookup[std::max(0, i)];
    float sin_next = sin_lookup[std::min(i+1, 1024)];

    return mix(sin_prev, sin_next, frac);
}

//---------------------------------------------------------------------------||

uint8_t hashI( int p ) {
    short h = p ^ (p >> 16);

    // rng is 64x64 = 12bits
    return rng[h & 255][(h >> 8) & 255];
}

static float hash1d( float p ) {
    // -1 to 1, consistent based on p
    int px = reinterpret_cast<int&>(p);
    float h = hashI(px);
    return (h - 128) * 0.0078125; // = 1 / 128.0;
}

static float noise1d( float p ) {
    // -1 to 1, consistent based on p
    //  perlin noise / warp-ish
    float i = floor( p );
    float f = p - i;

    float u = f*f*(3.0f - 2.0f * f);

    return mix( hash1d( i + 0.0f ),
                hash1d( i + 1.0f ), u);
}

static float hash2d( vec2 p ) {
    // -1 to 1, consistent based on p
    int px = reinterpret_cast<int&>(p.x);
    int py = reinterpret_cast<int&>(p.y);
    float h = hashI(px ^ py) ^ hashI(px & py);
    return (h - 128) * 0.0078125; // = 1 / 128.0;
}

static float noise2d( vec2 p ) {
    // -1 to 1, consistent based on p
    //  perlin noise / warp-ish
    vec2 i = floor( p );
    vec2 f = fract( p );

    vec2 u = f*f*(3.0f - 2.0f * f);

    return mix( mix( hash2d( i + vec2(0.0f ,0.0f) ),
                     hash2d( i + vec2(1.0f ,0.0f) ), u.x),
                mix( hash2d( i + vec2(0.0f ,1.0f) ),
                     hash2d( i + vec2(1.0f ,1.0f) ), u.x), u.y);
}

//---------------------------------------------------------------------------||

void meteorSetup() {
    global_i = 0;
    global_rf = 0; // Reset time till next lightning

    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++)
        for (int slot = 0; slot < MAX_METEORS; slot++)
            meteors[strip_i][slot].active = 0;
}

void meteorShowerStep() {
//    struct meteor {
//        bool active;
//        uint8_t wheel_i;
//        int16_t speed;
//        uint8_t length;
//        uint8_t sin;
//        float   pos_x;
//    }
//
//    meteor meteors[NUM_STRIPS][MAX_METEORS];

    const int MAX_POS = 1.2 * NUM_LEDS;

    // Time till next star with average STAR_RATE
    global_rf -= global_tDelta;
    if (global_rf < 0) {
        int strip_i = random(NUM_STRIPS);
        for (int slot = 0; slot < MAX_METEORS; slot++) {
            meteor* m = &meteors[strip_i][slot];

            if (m->active == 0) {

                m->active = 1;
                m->speed  = 18 + random(72);
                // BASE_SIZE + length / 10.0
                // 3.5 * ([0,1] + [0,2] * based on speed))
                m->length = 35 * (random_float() + 2*random_float()*m->speed/94.0);
                m->sin    = random(4);
                m->pos_x  = 0.0;

                // Random how much color changes per step (not currently used)
                //m->wheel_i_delta = 3*256 + random(2*256);

                // Accumulated counter.
                global_i += (m-> speed >> 3) + 2;
                // How much we shift the color each star (speed adds 1 to 11 | global_i adds 2)

                color_a_wheel_i = (global_i * color_delta_mult) / 32;
                m->wheel_i = color_a_wheel_i & 65535;

                break;
            }
        }
        global_rf = (0.4 + 1.6 * random_float()) * STAR_RATE / NUM_STRIPS * 32 / global_s;
    }


    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        if ((active_strips & (1 << strip_i)) == 0)
            continue;

        const int num_leds = NUM_LEDS;

        // TODO could be interesting to very slight blue / black backgrounds
        const CRGB background = CRGB( 0, 0, 0 );
        CRGB colors[num_leds] = {};
        for (int i = 0; i < num_leds; i++) {
            colors[i] = background;
        }

        for (int slot = 0; slot < MAX_METEORS; slot++) {
            meteor *m = &meteors[strip_i][slot];
            if (!m->active) continue;

            // Only change every couple of frames.
            /*
            if ((global_frames % 6) == 0) {
                m->wheel_i += m->wheel_i_delta;

                // Try to keep m->wheel_i "near" 0
                if (m->wheel_i > 250 || m->wheel_i < 180) {
                    m->wheel_i = 215;
                }
            }
            */

            uint8_t i_speed = m->speed;
            const float star_speed = i_speed * global_tDelta * STAR_VELOCITY / 42;
            const float pos = (m->pos_x += star_speed);

            float percent_strip = pos / NUM_LEDS;
            float adjust = pow(1 + percent_strip, 2);

            // Sin from 20% to 100%, larger adjust and period = faster = more cycles
            const float cycle_brightness = 0.6f + 0.4f * fast_sin(adjust * (1 + m->sin));

            if (pos > MAX_POS) {
                m->active = 0;
                continue;
            }

            float this_star_size = STAR_SIZE * NUM_LEDS + (m->length / 10.0);

            // assume tail is from opposite speed direction
            int start_i, end_i;

            // TODO handle prelighting start_i + 1 so that it's less "jumpy" as it crosess integer numbers
            // Be careful to keep (i - pos)/size can be outside [0,1]
            start_i = std::ceil(pos);
            end_i   = std::floor(pos + this_star_size);
            end_i   = std::min(end_i, NUM_LEDS-1);

            for (int i = start_i; i <= end_i; i++) {
                // brightness is (1 to 0) over START_SIZE, square diminishing
                float dist_normalized = (i - pos) / this_star_size;
                //uint32_t fade_brightness = 255 * dist_normalized * dist_normalized;
                //fade_brightness *= cycle_brightness;
                uint32_t fade_brightness = 255 * cycle_brightness * pow(dist_normalized, 2.5);

                CRGB m_color = ColorMap(m->wheel_i).nscale8(fade_brightness);

                if (colors[i] == background) {
                    colors[i] = m_color;
                } else {
                    // Blend equally
                    colors[i] = blend(colors[i], m_color, 128);
                }
            }
        }

        for (int i = 0; i < num_leds; i++) {
            setSinglePixel(strip_i, i, colors[i]);
        }
    }
}

void colliderSetup() {
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        // slot 0 is meteor
        meteor* m = &meteors[strip_i][0];
        m->active = 1;
        m->wheel_i = 25; // wheel_i = weight = light so it bounces more
        m->speed = 1000;
        m->length = 0;
        m->sin = 0;
        m->pos_x = NUM_LEDS / 2;

        for (int slot = 1; slot < COLLIDER_BALLS; slot++) {
            meteor* b = &meteors[strip_i][slot];
            b->active = 0;
            b->wheel_i = 10 + random(60);   // weight and length
            b->speed = -COLLIDER_MAX_SPEED + random(2 * COLLIDER_MAX_SPEED);
            m->length = 20;
            b->sin = 20;
            b->pos_x = (NUM_LEDS-1) * random_float();
        }
    }
}

void colliderStep() {
//    struct meteor {
//        bool active;
//        uint16_t wheel_i;
//        int16_t speed;
//        uint8_t length;
//        uint8_t sin;
//        float   pos_x;
//    }
//
//    meteor meteors[NUM_STRIPS][MAX_METEORS];

    // active = direction
    // wheel_i = weight and length
    //
    // speed = speed / 100
    // sin, length = time to live

    // Check if any ball has expired
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        if ((active_strips & (1 << strip_i)) == 0)
            continue;
        for (int slot = 1; slot < COLLIDER_BALLS; slot++) {
            meteor* b = &meteors[strip_i][slot];

            b->length--;
            if (b->length == 0) {
                b->length = 255;
                b->sin --;

                if (b-> sin == 0) {
                    // Reset collider
                    b->speed = -2000 + random(4000);
                    b->wheel_i = 10 + random(90);
                    b->sin = 20;
                }
            }
        }
    }

    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        if ((active_strips & (1 << strip_i)) == 0)
            continue;

        const int num_leds = NUM_LEDS;
        CRGB colors[num_leds] = {};
        for (int i = 0; i < num_leds; i++) {
            colors[i] = CRGB( 0, 0, 0 );
        }

        // Backwards so slot=0 on top.
        for (int slot = COLLIDER_BALLS - 1; slot >= 0; slot--) {
            //uint32_t slot_color = ColorMap(color_a_wheel_i - (NUM_STRIPS * COLLIDER_BALLS)/2 + 2 * (strip_i + NUM_STRIPS * slot));
            CRGB slot_color = ColorMap(200 * 256 * (strip_i * COLLIDER_BALLS + slot));

            meteor* ball = &meteors[strip_i][slot];

            // is case something wrong happens.
            if (ball->pos_x < -1 || ball->pos_x > (NUM_LEDS + 1)) {
                ball->pos_x = NUM_LEDS / 2;
            }

            // Check if colliders with any other later ball
            for (int test = slot+1; test < COLLIDER_BALLS; test++) {
                meteor* b2 = &meteors[strip_i][test];

                int delta_speed = ball->speed - b2->speed;
                float cross_time = (b2->pos_x - ball->pos_x) / (0.01 * delta_speed);
                if (0 <= cross_time && cross_time <= global_tDelta) {
                    // Collision!

                    // Magic elastic formula
                    int joint_mass = ball->wheel_i + b2->wheel_i;

                    int vA = (ball->wheel_i - b2->wheel_i) * ball->speed + (2 * b2->wheel_i) * b2->speed;
                    int vB = (b2->wheel_i - ball->wheel_i) * b2->speed + (2 * ball->wheel_i) * ball->speed;
                    vA /= joint_mass;
                    vB /= joint_mass;

                    // Fuck physics perpetual motion machine
                    vA += vA / 20;
                    vB += vB / 20;

                    ball->speed = constrain(vA, -COLLIDER_MAX_SPEED, COLLIDER_MAX_SPEED);
                    //if (slot == 0) {
                    //    if (-300 < ball->speed && ball->speed > 300) {
                    //        ball->speed = (ball->speed < 0) ? -300: 300;
                    //    }
                    //}
                    b2->speed = constrain(vB, -COLLIDER_MAX_SPEED, COLLIDER_MAX_SPEED);

                }
            }

            float pos_delta = ball->speed * 0.01 * global_tDelta;

            // if ball collides with start or end
            if (((ball->pos_x + pos_delta) < 0) || ((ball->pos_x + pos_delta) > (NUM_LEDS-1))) {
                ball->speed *= -1;
                pos_delta *= -1;
            }

            const float pos = (ball->pos_x += pos_delta);

            float trail_size = COLLIDER_MIN_SIZE;
            trail_size += ball->wheel_i * 0.06666f; // 1/15 | heaver balls -> larger

            int start_i, end_i;
            if (ball->speed > 0) {
                start_i = floor(pos - trail_size);
                end_i   = floor(pos);
            } else {
                start_i = ceil(pos);
                end_i   = ceil(pos + trail_size);
            }

            for (int i = start_i; i < end_i; i++) {
                CRGB m_color = slot_color;

                if (i >= 0 && i < NUM_LEDS) {// check if inside the LED strip
                    if ((colors[i].r + colors[i].g + colors[i].b) == 0) {
                        colors[i] = m_color;
                    }
                }
            }
        }

        for (int i = 0; i < num_leds; i++) {
            setSinglePixel(strip_i, i, colors[i]);
        }
    }
}

/*
 * Both reference this structure
    struct twinkle {
        bool active;
        uint16_t wheel_i;
        uint8_t position;
        uint8_t amount;
    };
*/
void twinkleSetup() {
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        for (int slot = 1; slot < MAX_TWINKLE; slot++) {
            twinkle *t = &twinkles[strip_i][slot];
            t->active = 0;
            t->wheel_i = 256 * 10 + random(60*256);
            t->position = 0;
        }
    }
}

void twinkleStep() {
    // Based on OMBRE wander above TODO account for "speed" somehow
    const float wander = 1100 * noise2d( vec2( global_t * 0.041f, global_t * 0.047f ) );

    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        // Check if should activate a new twinkle
        float rand = TWINKLE_RATE * global_tDelta / NUM_STRIPS;
        if (random_float() < rand) {
            for (int slot = 0; slot < MAX_TWINKLE; slot++) {
                twinkle *t = &twinkles[strip_i][slot];
                if (t->active == 0) {
                    t->active = 1;
                    t->amount = 100 + random(155);
                    t->wheel_i = 256 * wander;
                    t->position = random(NUM_LEDS);
                    break;
                }
            }
        }
    }

    // Set everything black
    setStrip(CRGB::Black);

    // Make use of correct calls and things
    for (int strip_i = 0; strip_i < NUM_STRIPS; strip_i++) {
        for (int slot = 0; slot < MAX_TWINKLE; slot++) {
            twinkle *t = &twinkles[strip_i][slot];
            if (t->active == 1) {
                CRGB color = ColorMap(t->wheel_i).nscale8(t->amount);
                setSinglePixel(strip_i, t->position, color);

                // Amount of fade per cycle
                t->amount -= TWINKLE_FADE;
                if (t->amount <= TWINKLE_FADE) {
                    t->active = 0;
                }
            }
        }
    }
}
