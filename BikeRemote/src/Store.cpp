#include "Store.h"
#include "AppState.h"
#include <Preferences.h>
#include <string.h>

static const char* NS = "bikeremote";

struct TargetRec {
  char addr[18];
  uint8_t addrType;
  char name[DEV_NAME_MAX + 1];
};

bool storeLoadCal(float m[6]) {
  Preferences prefs;
  prefs.begin(NS, true);
  size_t got = prefs.getBytes("cal", m, 6 * sizeof(float));
  prefs.end();
  return got == 6 * sizeof(float);
}

void storeSaveCal(const float m[6]) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putBytes("cal", m, 6 * sizeof(float));
  prefs.end();
}

void storeLoadTarget() {
  Preferences prefs;
  prefs.begin(NS, true);
  TargetRec rec;
  size_t got = prefs.getBytes("target", &rec, sizeof(rec));
  prefs.end();
  if (got != sizeof(rec)) return;
  app.haveTarget = true;
  memcpy(app.targetAddr, rec.addr, sizeof(app.targetAddr));
  app.targetAddr[sizeof(app.targetAddr) - 1] = 0;
  app.targetType = rec.addrType;
  memcpy(app.deviceName, rec.name, sizeof(app.deviceName));
  app.deviceName[DEV_NAME_MAX] = 0;
}

void storeSaveTarget() {
  if (!app.haveTarget) return;
  TargetRec rec = {};
  memcpy(rec.addr, app.targetAddr, sizeof(rec.addr));
  rec.addrType = app.targetType;
  memcpy(rec.name, app.deviceName, sizeof(rec.name));
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putBytes("target", &rec, sizeof(rec));
  prefs.end();
}

void storeForgetTarget() {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.remove("target");
  prefs.end();
  app.haveTarget = false;
  app.targetAddr[0] = 0;
  app.deviceName[0] = 0;
}
