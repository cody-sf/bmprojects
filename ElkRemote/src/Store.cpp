#include "Store.h"
#include "AppState.h"
#include <Preferences.h>
#include <string.h>

static const char* NS = "elkremote";

struct BarRec {
  char addr[18];
  uint8_t addrType;
  char label[LABEL_MAX + 1];
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

void storeClearCal() {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.remove("cal");
  prefs.end();
}

void storeLoadBars() {
  Preferences prefs;
  prefs.begin(NS, true);
  BarRec recs[MAX_BARS];
  size_t got = prefs.getBytes("bars", recs, sizeof(recs));
  prefs.end();
  int n = got / sizeof(BarRec);
  for (int i = 0; i < n && i < MAX_BARS; i++) {
    Bar& bar = app.bars[i];
    bar.used = true;
    memcpy(bar.addr, recs[i].addr, sizeof(bar.addr));
    bar.addr[sizeof(bar.addr) - 1] = 0;
    bar.addrType = recs[i].addrType;
    memcpy(bar.label, recs[i].label, sizeof(bar.label));
    bar.label[LABEL_MAX] = 0;
  }
}

void storeSaveBars() {
  BarRec recs[MAX_BARS];
  int n = 0;
  for (int i = 0; i < MAX_BARS; i++) {
    if (!app.bars[i].used) continue;
    memcpy(recs[n].addr, app.bars[i].addr, sizeof(recs[n].addr));
    recs[n].addrType = app.bars[i].addrType;
    memcpy(recs[n].label, app.bars[i].label, sizeof(recs[n].label));
    n++;
  }
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putBytes("bars", recs, n * sizeof(BarRec));
  prefs.end();
}

void storeForgetBars() {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.remove("bars");
  prefs.end();
  for (int i = 0; i < MAX_BARS; i++) {
    app.bars[i] = Bar();
  }
}
