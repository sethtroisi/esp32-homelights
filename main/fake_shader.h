#ifndef SHADER_H
#define SHADER_H

#pragma once

#include <cmath>
#include <cstdint>

//---------------------------------------------------------------------------||

float mix(float x, float y, float a) {
  return x * (1 - a) + y * a;
}

uint8_t mix8(uint8_t x, uint8_t y, uint8_t a) {
    uint16_t X = x, Y = y;
    uint16_t t = (X * (255 - a) + Y * a);
    return (t >> 8) & 255;
}

//--------------------------------------------------------------------------||

class vec2;

class vec2 {
    public:
        float x;
        float y;

        vec2(float a): x(a), y(a) {}
        vec2(float a, float b): x(a), y(b) {}

        vec2 operator+(float s) const {
            return { this->x + s, this->y + s };
        }

        vec2 operator*(float s) const {
            return { this->x * s, this->y * s};
        }

        vec2 operator+(const vec2 &s) const {
            return { this->x + s.x, this->y + s.y };
        }

        vec2 operator-(const vec2 &s) const {
            return { this->x - s.x, this->y - s.y };
        }

        vec2 operator*(const vec2 &s) const {
            return { this->x * s.x, this->y * s.y };
        }
};

vec2 operator +(const float a, const vec2 &b) { return b + a; }
vec2 operator -(const float a, const vec2 &b) { return { a - b.x, a - b.y}; }
vec2 operator *(const float a, const vec2 &b) { return b * a; }


vec2 floor(vec2 a) {
  return { (float)floor((float) a.x), (float)floor((float) a.y) };
}


vec2 fract(vec2 a) {
  return a - floor(a);
}

//--------------------------------------------------------------------------||

#endif /* SHADER_H */
