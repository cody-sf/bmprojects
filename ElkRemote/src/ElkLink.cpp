#include "ElkLink.h"
#include "AppState.h"
#include "ElkCodec.h"
#include "Store.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

struct ElkWrite {
  uint8_t barIdx;
  uint8_t frame[ELK_FRAME_LEN];
};

static QueueHandle_t writeQueue = nullptr;
static NimBLEClient* clients[MAX_BARS] = {nullptr};
static NimBLERemoteCharacteristic* writeChars[MAX_BARS] = {nullptr};

// Bits set by the scan callback (host task), consumed by the BLE task.
static volatile uint8_t pendingMask = 0;
static volatile bool forgetRequested = false;
static volatile uint32_t forceScanUntil = 0;

static int findBarByAddr(const char* addr) {
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && strcasecmp(app.bars[i].addr, addr) == 0) {
      return i;
    }
  }
  return -1;
}

static int findBarByClient(NimBLEClient* client) {
  for (int i = 0; i < MAX_BARS; i++) {
    if (clients[i] == client) return i;
  }
  return -1;
}

/**
 * Default label, matching elkBarLabel in RNUmbrella/helpers.ts: the text
 * after the advertised name's first space, else the last 4 of the MAC.
 */
static void defaultLabel(char* out, const std::string& advertised, const char* addr) {
  size_t space = advertised.find(' ');
  if (space != std::string::npos) {
    std::string suffix = advertised.substr(space + 1);
    while (!suffix.empty() && suffix.front() == ' ') suffix.erase(0, 1);
    while (!suffix.empty() && suffix.back() == ' ') suffix.pop_back();
    if (!suffix.empty()) {
      snprintf(out, LABEL_MAX + 1, "Bar %s", suffix.c_str());
      return;
    }
  }
  char last4[5] = {0};
  int n = 0;
  for (int i = strlen(addr) - 1; i >= 0 && n < 4; i--) {
    if (addr[i] != ':') last4[n++] = toupper(addr[i]);
  }
  // Collected backwards; flip.
  for (int i = 0; i < n / 2; i++) {
    char tmp = last4[i];
    last4[i] = last4[n - 1 - i];
    last4[n - 1 - i] = tmp;
  }
  snprintf(out, LABEL_MAX + 1, "Bar %s", last4);
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    std::string name = device->getName();
    if (name.length() < 4 || strncasecmp(name.c_str(), "ELK-", 4) != 0) {
      return;
    }
    std::string addr = device->getAddress().toString();
    int idx = findBarByAddr(addr.c_str());
    if (idx < 0) {
      // Auto-adopt, exactly like the app: no setup screen, a bar we can see
      // is a bar we own.
      for (int i = 0; i < MAX_BARS; i++) {
        if (!app.bars[i].used) {
          idx = i;
          break;
        }
      }
      if (idx < 0) return;  // registry full
      Bar& bar = app.bars[idx];
      strlcpy(bar.addr, addr.c_str(), sizeof(bar.addr));
      bar.addrType = device->getAddress().getType();
      defaultLabel(bar.label, name, bar.addr);
      bar.used = true;
      storeSaveBars();
      app.connectionsDirty = true;
    }
    if (!app.bars[idx].connected) {
      pendingMask |= (1 << idx);  // BLE task connects; never from a callback
    }
  }
};

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* client, int reason) override {
    int idx = findBarByClient(client);
    if (idx >= 0) {
      app.bars[idx].connected = false;
      writeChars[idx] = nullptr;
      app.connectionsDirty = true;
    }
  }
};

static ScanCallbacks scanCallbacks;
static ClientCallbacks clientCallbacks;

static bool anyMissing() {
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && !app.bars[i].connected) return true;
  }
  return false;
}

static void connectBar(int idx) {
  Bar& bar = app.bars[idx];
  if (!clients[idx]) {
    clients[idx] = NimBLEDevice::createClient();
    if (!clients[idx]) return;
    clients[idx]->setClientCallbacks(&clientCallbacks, false);
    clients[idx]->setConnectTimeout(5000);
    // Relaxed intervals: 8 links plus a scan share one radio.
    clients[idx]->setConnectionParams(24, 48, 0, 400);
  }
  NimBLEAddress addr(std::string(bar.addr), bar.addrType);
  if (!clients[idx]->connect(addr)) {
    return;  // bar is off or out of range; the scan will see it again
  }
  NimBLERemoteService* service = clients[idx]->getService(ELK_SERVICE_UUID);
  NimBLERemoteCharacteristic* chr = service ? service->getCharacteristic(ELK_WRITE_UUID) : nullptr;
  if (!chr) {
    // ELK- name but not the bar protocol - leave it alone.
    clients[idx]->disconnect();
    return;
  }
  writeChars[idx] = chr;
  bar.connected = true;
  app.connectionsDirty = true;
}

static void doWrite(const ElkWrite& w) {
  int idx = w.barIdx;
  if (idx < 0 || idx >= MAX_BARS) return;
  NimBLERemoteCharacteristic* chr = writeChars[idx];
  if (!app.bars[idx].connected || !chr) return;
  chr->writeValue(w.frame, ELK_FRAME_LEN, !chr->canWriteNoResponse());
}

static void bleTask(void*) {
  NimBLEDevice::init("ElkRemote");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setActiveScan(true);  // the name rides the scan response
  scan->setMaxResults(0);     // callbacks only; nothing stored

  for (;;) {
    ElkWrite w;
    while (xQueueReceive(writeQueue, &w, pdMS_TO_TICKS(20)) == pdTRUE) {
      doWrite(w);
    }

    if (forgetRequested) {
      forgetRequested = false;
      pendingMask = 0;
      if (scan->isScanning()) scan->stop();
      for (int i = 0; i < MAX_BARS; i++) {
        if (clients[i] && clients[i]->isConnected()) clients[i]->disconnect();
        writeChars[i] = nullptr;
      }
      storeForgetBars();
      app.connectionsDirty = true;
      continue;
    }

    if (pendingMask) {
      if (scan->isScanning()) scan->stop();
      for (int i = 0; i < MAX_BARS; i++) {
        if (pendingMask & (1 << i)) {
          pendingMask &= ~(1 << i);
          if (app.bars[i].used && !app.bars[i].connected) {
            connectBar(i);
          }
          break;  // one per pass; writes stay responsive between connects
        }
      }
      continue;
    }

    bool wantScan = anyMissing() || (int32_t)(forceScanUntil - millis()) > 0;
    if (wantScan && !scan->isScanning()) {
      scan->start(0, false, true);  // forever; stopped when everyone is home
    } else if (!wantScan && scan->isScanning()) {
      scan->stop();
    }
  }
}

void elkInit() {
  storeLoadBars();
  writeQueue = xQueueCreate(48, sizeof(ElkWrite));
  xTaskCreatePinnedToCore(bleTask, "elk_ble", 6144, nullptr, 2, nullptr, 0);
}

void elkSend(int barIdx, const uint8_t* frame) {
  if (!writeQueue || barIdx < 0 || barIdx >= MAX_BARS) return;
  ElkWrite w;
  w.barIdx = barIdx;
  memcpy(w.frame, frame, ELK_FRAME_LEN);
  xQueueSend(writeQueue, &w, 0);  // full queue drops; these are best-effort
}

void elkRequestScan() {
  forceScanUntil = millis() + 12000;
}

bool elkIsScanning() {
  return NimBLEDevice::getScan()->isScanning();
}

void elkForgetAll() {
  forgetRequested = true;
}
