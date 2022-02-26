#pragma once

//--------------------------------------------------------------------------||
// Main command processor

#include "Particle.h"

void logString(string key);
void logKeyValue(string key, string value);
void logValue(string key, float value);

void hl_setup();
void hl_loop();
