#pragma once
#include <stdint.h>

/**
 * NVS persistence (Preferences namespace "bikeremote"): the touch calibration
 * affine and the one saved target device (address + type + last known name).
 * Device state is never stored - the light itself is the source of truth and
 * reports on connect.
 */

// screenX = m[0]*rawX + m[1]*rawY + m[2]; screenY = m[3]*rawX + m[4]*rawY + m[5]
bool storeLoadCal(float m[6]);
void storeSaveCal(const float m[6]);

void storeLoadTarget();
void storeSaveTarget();
void storeForgetTarget();
