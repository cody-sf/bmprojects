#include "BmLink.h"
#include "AppState.h"
#include "BmCodec.h"
#include "Catalog.h"
#include "Store.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

// Mirrors BMDEVICE_UUIDS in RNUmbrella/constants.ts / BMProfile.bmDevice on
// the watch.
static const char* BM_SERVICE_UUID = "4746abe4-2135-4a84-8f2f-f47f3a73e73b";
static const char* BM_FEATURES_UUID = "3927e9db-012b-4db9-8890-984fe28faf83";
static const char* BM_STATUS_UUID = "c054c450-cf93-4e6f-848e-2c521e739f4b";

struct BmWrite {
  uint8_t len;
  uint8_t data[BM_WRITE_MAX];
};

static QueueHandle_t writeQueue = nullptr;
static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* featuresChar = nullptr;

// Set by scan/UI, consumed by the BLE task.
static volatile bool targetSeen = false;
static uint8_t seenAddrType = 0;
static volatile bool discoverMode = false;
static volatile int chooseRequest = -1;
static volatile bool forgetRequested = false;
static uint32_t connectedAt = 0;
static bool statusRetried = false;

/**
 * Display name, matching resolveDeviceName in RNUmbrella/helpers.ts and
 * BMNaming.resolve on the watch: the generic "BMDevice" identifier leads the
 * advertised name for scan matching but is never shown; real words survive.
 */
static void cleanName(char* out, size_t outLen, const char* advertised, const char* addr) {
  const char* p = advertised;
  if (strncasecmp(p, "BMDevice", 8) == 0) {
    p += 8;
    while (*p == ' ' || *p == '-' || *p == '_') p++;
  }
  if (*p) {
    strlcpy(out, p, outLen);
    return;
  }
  char last4[5] = {0};
  int n = 0;
  for (int i = strlen(addr) - 1; i >= 0 && n < 4; i--) {
    if (addr[i] != ':') last4[n++] = toupper(addr[i]);
  }
  for (int i = 0; i < n / 2; i++) {
    char tmp = last4[i];
    last4[i] = last4[n - 1 - i];
    last4[n - 1 - i] = tmp;
  }
  snprintf(out, outLen, "Light %s", last4);
}

// ---------------------------------------------------------------- status parse

static int effectIndexForId(const char* id) {
  for (int i = 0; i < EFFECT_COUNT; i++) {
    if (strcasecmp(EFFECTS[i].id, id) == 0) return i;
  }
  return -1;
}

static int16_t paletteSelForId(const char* id) {
  for (int i = 0; i < PALETTE_COUNT; i++) {
    if (strcasecmp(PALETTES[i].id, id) == 0) return i;
  }
  // "custom1".."custom4" resolve against the device's own slots.
  if (strncasecmp(id, "custom", 6) == 0 && id[6] >= '1' && id[6] <= '0' + CUSTOM_PALETTE_COUNT) {
    return CUSTOM_SEL_BASE + (id[6] - '1');
  }
  return -1;
}

static void applyParam(JsonObject o, const char* key, uint8_t code) {
  if (!o[key].is<int>()) return;
  int slot = paramSlot(code);
  if (slot >= 0) app.params[slot] = o[key].as<int>();
}

static void applyCustomPaletteChunk(JsonObject o) {
  int slot = o["i"] | -1;
  if (slot < 0 || slot >= CUSTOM_PALETTE_COUNT) return;
  CustomPalette& cp = app.customs[slot];
  const char* name = o["n"] | "";
  const char* packed = o["c"] | "";
  // An empty name means an empty slot - how a deletion made on the phone
  // reaches this remote.
  if (!name[0] || strlen(packed) < CUSTOM_PALETTE_ENTRIES * 6) {
    cp.used = false;
    return;
  }
  strlcpy(cp.name, name, sizeof(cp.name));
  for (int i = 0; i < CUSTOM_PALETTE_ENTRIES; i++) {
    char hex[7] = {0};
    memcpy(hex, packed + i * 6, 6);
    uint32_t v = strtoul(hex, nullptr, 16);
    cp.colors[i][0] = (v >> 16) & 0xFF;
    cp.colors[i][1] = (v >> 8) & 0xFF;
    cp.colors[i][2] = v & 0xFF;
  }
  cp.used = true;
}

/** Merge one status chunk. Only keys actually present may be applied. */
static void applyStatusJson(const uint8_t* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
    // The classic cause is a chunk outgrowing the ~239-byte notify payload
    // and arriving clipped (how the matrix keys went missing when they rode
    // devConfig) - make it loud instead of a silently absent feature.
    Serial.printf("[BmLink] status chunk failed to parse (%u bytes): %.32s...\n",
                  (unsigned)len, (const char*)data);
    return;
  }
  JsonObject o = doc.as<JsonObject>();
  if (o.isNull()) return;

  if (o["pwr"].is<bool>()) app.power = o["pwr"];
  if (o["bri"].is<int>()) app.brightness = constrain(o["bri"].as<int>(), 1, 100);
  if (o["spd"].is<int>()) app.speedPct = speedRawToPct(o["spd"].as<int>());
  if (o["dir"].is<bool>()) app.reversed = o["dir"];
  if (o["maxBri"].is<int>()) app.maxBrightness = constrain(o["maxBri"].as<int>(), 1, 100);
  if (o["gps"].is<bool>()) app.gpsAvailable = o["gps"];
  if (o["sync"].is<bool>()) app.syncEnabled = o["sync"];
  if (o["mtxW"].is<int>()) app.mtxW = o["mtxW"];
  if (o["mtxH"].is<int>()) app.mtxH = o["mtxH"];
  if (o["marquee"].is<const char*>()) {
    strlcpy(app.marquee, o["marquee"], sizeof(app.marquee));
  }
  if (o["bmp"].is<bool>()) app.hasBitmap = o["bmp"];
  if (o["txtSty"].is<int>()) app.boldText = o["txtSty"].as<int>() != 0;
  if (o["txtFill"].is<int>()) app.textFill = constrain(o["txtFill"].as<int>(), 0, 3);
  if (o["anim"].is<int>()) app.animFrames = o["anim"];
  if (o["disp"].is<int>()) app.matrixDisplay = (uint8_t)constrain(o["disp"].as<int>(), 0, 4);

  if (o["fx"].is<const char*>()) {
    int idx = effectIndexForId(o["fx"]);
    if (idx >= 0) app.effectIdx = idx;
  }
  if (o["pal"].is<const char*>()) {
    int16_t sel = paletteSelForId(o["pal"]);
    if (sel >= 0) app.paletteSel = sel;
  }
  if (o["deviceName"].is<const char*>()) {
    const char* name = o["deviceName"];
    // The factory "BMDevice" is a placeholder both apps fall back from.
    if (name[0] && strcasecmp(name, "BMDevice") != 0 &&
        strcmp(app.deviceName, name) != 0) {
      strlcpy(app.deviceName, name, sizeof(app.deviceName));
      storeSaveTarget();  // header reads right on next boot, before connecting
    }
  }

  // effectParams chunk - the Tweaks sliders track the device, not guesses.
  applyParam(o, "ww", 0x0B);
  applyParam(o, "mc", 0x0C);
  applyParam(o, "tl", 0x0D);
  applyParam(o, "hv", 0x0E);
  applyParam(o, "mir", 0x0F);
  applyParam(o, "cc", 0x10);
  applyParam(o, "dr", 0x11);
  applyParam(o, "cs", 0x12);
  applyParam(o, "bc", 0x13);
  applyParam(o, "wc", 0x14);
  applyParam(o, "fi", 0x15);
  applyParam(o, "ff", 0x16);
  applyParam(o, "es", 0x17);
  applyParam(o, "sa", 0x18);
  if (o["col"].is<JsonObject>()) {
    JsonObject col = o["col"];
    app.colR = col["r"] | app.colR;
    app.colG = col["g"] | app.colG;
    app.colB = col["b"] | app.colB;
  }

  if (strcmp(o["type"] | "", "cpal") == 0) applyCustomPaletteChunk(o);

  app.hasStatus = true;
  app.statusDirty = true;
}

static void onStatusNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  applyStatusJson(data, len);
}

// ---------------------------------------------------------------- scanning

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    bool isBm = device->isAdvertisingService(NimBLEUUID(BM_SERVICE_UUID));
    std::string name = device->getName();
    if (!isBm && strncasecmp(name.c_str(), "BMDevice", 8) != 0) return;

    std::string addr = device->getAddress().toString();

    if (discoverMode) {
      int slot = -1;
      for (int i = 0; i < FOUND_MAX; i++) {
        if (app.found[i].used && strcasecmp(app.found[i].addr, addr.c_str()) == 0) {
          slot = i;
          break;
        }
        if (slot < 0 && !app.found[i].used) slot = i;
      }
      if (slot >= 0) {
        FoundDevice& f = app.found[slot];
        bool fresh = !f.used;
        f.used = true;
        strlcpy(f.addr, addr.c_str(), sizeof(f.addr));
        f.addrType = device->getAddress().getType();
        f.rssi = device->getRSSI();
        if (fresh || name.length()) {
          cleanName(f.name, sizeof(f.name), name.c_str(), f.addr);
        }
        if (fresh) app.foundDirty = true;
      }
    }

    if (app.haveTarget && !app.connected &&
        strcasecmp(app.targetAddr, addr.c_str()) == 0) {
      seenAddrType = device->getAddress().getType();
      targetSeen = true;  // BLE task connects; never from a callback
    }
  }
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int reason) override {
    featuresChar = nullptr;
    app.connected = false;
    app.hasStatus = false;
    app.connectionsDirty = true;
  }
};

static ScanCallbacks scanCallbacks;
static ClientCallbacks clientCallbacks;

// ---------------------------------------------------------------- connect

static void connectTarget() {
  if (!client) {
    client = NimBLEDevice::createClient();
    if (!client) return;
    client->setClientCallbacks(&clientCallbacks, false);
    client->setConnectTimeout(6000);
  }
  NimBLEAddress addr(std::string(app.targetAddr), seenAddrType);
  if (!client->connect(addr)) {
    return;  // out of range or busy; the scan will see it again
  }
  NimBLERemoteService* service = client->getService(BM_SERVICE_UUID);
  NimBLERemoteCharacteristic* features =
      service ? service->getCharacteristic(BM_FEATURES_UUID) : nullptr;
  NimBLERemoteCharacteristic* status =
      service ? service->getCharacteristic(BM_STATUS_UUID) : nullptr;
  if (!features || !status) {
    client->disconnect();
    return;
  }
  // Subscribe before asking: the firmware drops the status burst on the floor
  // unless the CCCD write has landed (isSubscribed() in BMBluetoothHandler).
  // subscribe() goes with response, so once it returns we are registered.
  if (!status->subscribe(true, onStatusNotify)) {
    client->disconnect();
    return;
  }
  featuresChar = features;
  app.connected = true;
  app.connectionsDirty = true;
  connectedAt = millis();
  statusRetried = false;

  uint8_t cmd[BM_WRITE_MAX];
  size_t len = bmEncodeBare(cmd, BM_CMD_REQUEST_STATUS);
  features->writeValue(cmd, len, true);
}

static void doWrite(const BmWrite& w) {
  NimBLERemoteCharacteristic* chr = featuresChar;
  if (!app.connected || !chr) return;
  chr->writeValue(w.data, w.len, true);  // with response only - see BmCodec.h
}

// ---------------------------------------------------------------- task

static void bleTask(void*) {
  NimBLEDevice::init("BikeRemote");
  // Status chunks run up to ~239 bytes; the default 23-byte ATT MTU would
  // truncate every one of them.
  NimBLEDevice::setMTU(255);
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setActiveScan(true);  // the name rides the scan response
  scan->setMaxResults(0);     // callbacks only; nothing stored

  for (;;) {
    BmWrite w;
    while (xQueueReceive(writeQueue, &w, pdMS_TO_TICKS(20)) == pdTRUE) {
      doWrite(w);
    }

    if (forgetRequested) {
      forgetRequested = false;
      targetSeen = false;
      if (client && client->isConnected()) client->disconnect();
      storeForgetTarget();
      app.connectionsDirty = true;
      continue;
    }

    int choose = chooseRequest;
    if (choose >= 0) {
      chooseRequest = -1;
      if (choose < FOUND_MAX && app.found[choose].used) {
        if (client && client->isConnected()) client->disconnect();
        FoundDevice& f = app.found[choose];
        app.haveTarget = true;
        strlcpy(app.targetAddr, f.addr, sizeof(app.targetAddr));
        app.targetType = f.addrType;
        strlcpy(app.deviceName, f.name, sizeof(app.deviceName));
        storeSaveTarget();
        seenAddrType = f.addrType;
        targetSeen = true;
        app.connectionsDirty = true;
      }
    }

    if (targetSeen) {
      targetSeen = false;
      if (app.haveTarget && !app.connected) {
        if (scan->isScanning()) scan->stop();
        connectTarget();
      }
      continue;
    }

    // The connect handshake asks for status once; if that burst was lost, one
    // retry covers it without turning into a poll (same as the watch).
    if (app.connected && !app.hasStatus && !statusRetried &&
        millis() - connectedAt > 1500) {
      statusRetried = true;
      uint8_t cmd[BM_WRITE_MAX];
      BmWrite retry;
      retry.len = bmEncodeBare(cmd, BM_CMD_REQUEST_STATUS);
      memcpy(retry.data, cmd, retry.len);
      doWrite(retry);
    }

    bool wantScan = discoverMode || (app.haveTarget && !app.connected);
    if (wantScan && !scan->isScanning()) {
      scan->start(0, false, true);  // forever; stopped when no longer wanted
    } else if (!wantScan && scan->isScanning()) {
      scan->stop();
    }
  }
}

// ---------------------------------------------------------------- API

void bmInit() {
  storeLoadTarget();
  writeQueue = xQueueCreate(24, sizeof(BmWrite));
  xTaskCreatePinnedToCore(bleTask, "bm_ble", 8192, nullptr, 2, nullptr, 0);
}

void bmQueueWrite(const uint8_t* data, size_t len) {
  if (!writeQueue || len == 0 || len > BM_WRITE_MAX) return;
  BmWrite w;
  w.len = len;
  memcpy(w.data, data, len);
  xQueueSend(writeQueue, &w, 0);  // full queue drops; these are best-effort
}

void bmSetDiscover(bool on) {
  if (on) {
    for (int i = 0; i < FOUND_MAX; i++) app.found[i] = FoundDevice();
    app.foundDirty = true;
  }
  discoverMode = on;
}

bool bmIsScanning() {
  return NimBLEDevice::getScan()->isScanning();
}

void bmChooseTarget(int idx) {
  chooseRequest = idx;
}

void bmForgetTarget() {
  forgetRequested = true;
}
