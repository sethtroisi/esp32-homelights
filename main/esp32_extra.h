#pragma once

//---------------------------------------------------------------------------||
//--------------- Utilities For Diff between Photon and esp32 ---------------||

#include <cmath>

//---------------------------------------------------------------------------||

int random(int max) {
    // TODO check if esp32 supports this
    return random() % max;
}

// TODO check for better definition
int constrain(int a, int min, int max) {
    return a <= min ? min : (a >= max ? max : a);
}
