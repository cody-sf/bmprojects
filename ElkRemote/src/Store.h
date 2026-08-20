#pragma once
#include <stdint.h>

/**
 * NVS persistence (Preferences namespace "elkremote"): the touch calibration
 * affine and the bar registry (address + type + label per bar). Runtime state
 * is never stored - the bars hold none and neither do we across boots.
 */

// screenX = m[0]*rawX + m[1]*rawY + m[2]; screenY = m[3]*rawX + m[4]*rawY + m[5]
bool storeLoadCal(float m[6]);
void storeSaveCal(const float m[6]);
void storeClearCal();

void storeLoadBars();
void storeSaveBars();
void storeForgetBars();
