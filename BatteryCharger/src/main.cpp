#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include <BMDevice.h>

// =============================================================================
// Battery Charger — 8-port single-cell Li-ion charge monitor
// =============================================================================
//
// Each port has a battery under charge behind a /2 voltage divider on an ADC
// pin, and an addressable WS2812 status LED. The ESP reads the port voltages,
// colours the LEDs by charge state, and reports every port to the RNUmbrella
// app over BLE (device type "batterycharger").
//
// This is built on the shared BMDevice framework for the BLE plumbing (chunked
// status, subscription gating, owner/name persistence, the app's request_status
// poll on 0x02). The status LEDs are driven directly with FastLED, deliberately
// NOT registered with the framework's LightShow, so the lighting engine never
// touches them.
//
// -----------------------------------------------------------------------------
// PORT MAP — verified against the EasyEDA schematic (rev 1.0, 2025-07-21).
// -----------------------------------------------------------------------------
// `adcPin`   : the ESP32 ADC pin wired to this port's /2 divider (BATT_VCC of
//              the CN3791 module -> 100k -> node -> 100k -> GND, node to adcPin).
// `ledIndex` : this port's position in the external WS2812 chain plugged into
//              the 5VLED header (0 = first LED the data line reaches). Mount the
//              strip so its DIN-end LED sits at port 1.
// `label`    : what the app shows for the port (matches the BATn silk).
//
// Order here is CH1/BAT1 (top of the board) -> CH8/BAT8 (bottom), which is the
// order the app displays ports in. Ports 5-8 (GPIO27/14/25/26) are ADC2 pins;
// they read fine here because this firmware never starts WiFi (ADC2 is only
// locked out by the WiFi driver, not by BLE).
struct PortMap {
  uint8_t adcPin;
  uint8_t ledIndex;
  const char* label;
};

static const PortMap PORTS[] = {
  {32, 0, "1"},  // CH1 / BAT1, divider R1
  {33, 1, "2"},  // CH2 / BAT2, divider R4
  {34, 2, "3"},  // CH3 / BAT3, divider R6
  {35, 3, "4"},  // CH4 / BAT4, divider R8
  {27, 4, "5"},  // CH5 / BAT5, divider R10
  {14, 5, "6"},  // CH6 / BAT6, divider R12
  {25, 6, "7"},  // CH7 / BAT7, divider R14
  {26, 7, "8"},  // CH8 / BAT8, divider R16
};
static const int NUM_PORTS = sizeof(PORTS) / sizeof(PORTS[0]);

// WS2812 status LEDs on the 5VLED header (pin1 +5V, pin2 DIN, pin3 GND).
// The schematic wires DIN to GPIO5.
#define STATUS_LED_PIN 5
#define STATUS_LED_BRIGHTNESS 60  // 0-255, global FastLED brightness for the box

// -----------------------------------------------------------------------------
// BLE identity. The service UUID is what the app matches on to know this is a
// battery charger (see BATTERYCHARGER_UUIDS in RNUmbrella/constants.ts) — keep
// the two in step.
#define DEVICE_NAME   "BatteryCharger"
#define SERVICE_UUID  "9b5a1c00-1f2e-4c3a-9b7d-2e6f5a4c8d10"
#define FEATURES_UUID "9b5a1c01-1f2e-4c3a-9b7d-2e6f5a4c8d10"
#define STATUS_UUID   "9b5a1c02-1f2e-4c3a-9b7d-2e6f5a4c8d10"

// Battery-charger feature commands (device-specific, so clear of the framework's
// common table).
#define BLE_FEATURE_GET_BATTERY       0x50  // trigger: push a battery status burst now
#define BLE_FEATURE_SET_CALIBRATION   0x51  // float LE: multimeter/ESP correction factor
#define BLE_FEATURE_RESET_CALIBRATION 0x52  // trigger: back to the factory factor
#define BLE_FEATURE_RESCAN_PORTS      0x53  // trigger: clear the "seen charging" latches

// -----------------------------------------------------------------------------
// ADC + divider. Battery voltage = (pin millivolts) * divider ratio * cal.
//
// The pin voltage is read with analogReadMilliVolts(), which applies the
// ESP32's factory eFuse ADC calibration (per-chip Vref + the ADC's nonlinear
// curve). That is far more accurate and stable than raw analogRead() scaled by a
// nominal 3.3V/4095, which is what made the numbers wander.
static const float VOLTAGE_DIVIDER_RATIO = 2.0f;  // 4.2V battery -> 2.1V at the pin
// Fine trim on top of the calibrated reading, for the 1% divider resistors.
// analogReadMilliVolts is already accurate, so this defaults to 1.0 (the old
// 1.073 corrected raw analogRead and is no longer needed). Persisted in NVRAM,
// adjustable live over BLE; reset to factory returns it to 1.0.
static const float DEFAULT_CALIBRATION = 1.0f;

// Classification thresholds, in millivolts at the battery.
//
// Key fact for THIS board: an empty port reads HIGHER than any battery. With no
// battery the CN3791 output is open-circuit and drifts up to the module's
// regulation setpoint - measured at ~4.25V here. A battery, even full, clamps the
// terminal to its own electrochemical voltage (<= ~4.2V), so it always reads
// below the empty float. That makes empty a simple threshold, and it means
// removal is detected for free: pull a battery and the port floats back to 4.25.
//
//   >= MV_FAULT        genuine over-voltage, above the empty float
//   >= MV_EMPTY_FLOAT  no battery - port floating at the ~4.25V setpoint
//   <  MV_ABSENT_LOW   dead cell / nothing driving the pin
//   otherwise          a battery is present (reachedFull picks charging vs full)
//
// MV_EMPTY_FLOAT must sit a little under the empty-port reading (~4.25V here) and
// above the most a real battery reaches (~4.2V). Retune if the board's float
// differs - watch an empty port's voltage in the app.
static const int MV_ABSENT_LOW   = 1500;
static const int MV_FULL_MARK    = 4180;  // a present battery at/above this is charged
static const int MV_EMPTY_FLOAT  = 4230;  // at/above: empty port floating (~4.25V, no battery)
static const int MV_FAULT        = 4400;  // above: genuine over-voltage

// Port charge state, reported to the app as the "st" array and used to colour
// the status LED. Keep in step with BATTERY_PORT_STATES in RNUmbrella/constants.ts.
enum PortState {
  PS_NONE     = 0,  // no battery present
  PS_CHARGING = 1,  // battery present, below full
  PS_FULL     = 2,  // charged
  PS_FAULT    = 3,  // over-voltage / out of range
};

// -----------------------------------------------------------------------------
// Filtering, in two stages:
//  1. Per read pass: a median of NUM_READINGS oversamples rejects switching-noise
//     spikes from the CN3791/MP1584 regulators.
//  2. Across passes: a moving average over the last MOVING_AVERAGE_SIZE medians
//     smooths the slow wander. A battery charges slowly, so heavy smoothing costs
//     nothing and steadies the displayed number.
static const int NUM_READINGS = 32;
static const int MOVING_AVERAGE_SIZE = 12;
static const unsigned long VOLTAGE_READ_INTERVAL = 1000;  // ms between read passes

// -----------------------------------------------------------------------------
// Runtime state.
BMDevice* bmDevice = nullptr;
Preferences calPrefs;
float calibrationFactor = DEFAULT_CALIBRATION;

CRGB statusLeds[NUM_PORTS];

float movingAverageBuffer[NUM_PORTS][MOVING_AVERAGE_SIZE];
int   bufferIndex[NUM_PORTS];
bool  bufferFilled[NUM_PORTS];

int       latestMilliVolts[NUM_PORTS];
PortState latestState[NUM_PORTS];
// Latched once a present battery reaches MV_FULL_MARK, so it keeps reading FULL
// as a finished battery relaxes below the charge voltage instead of flipping back
// to "charging". Cleared when the port reads empty (battery removed).
bool      reachedFull[NUM_PORTS];

unsigned long lastVoltageReadTime = 0;
bool faultBlinkOn = false;
unsigned long lastFaultBlink = 0;
const unsigned long FAULT_BLINK_INTERVAL = 400;  // ms

// Forward declarations
bool handleCustomFeature(uint8_t feature, const uint8_t* data, size_t length);
float readPortMilliVolts(int portIndex);
PortState classifyPort(int portIndex, int milliVolts);
void updateStatusLEDs();
void sendBatteryStatusChunk();
void loadCalibration();
void saveCalibration(float value);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Battery Charger — voltage monitor + BLE");

  // ADC pins are inputs, no pull-ups. 11dB attenuation gives a usable range of
  // ~0-2.45V at the pin, which covers our 2.1V full-charge point, and selects the
  // matching calibration curve for analogReadMilliVolts().
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < NUM_PORTS; i++) {
    pinMode(PORTS[i].adcPin, INPUT);
    bufferIndex[i] = 0;
    bufferFilled[i] = false;
    latestMilliVolts[i] = 0;
    latestState[i] = PS_NONE;
    reachedFull[i] = false;
    for (int j = 0; j < MOVING_AVERAGE_SIZE; j++) {
      movingAverageBuffer[i][j] = 0.0f;
    }
  }

  // Status LEDs are ours alone — added straight to FastLED, never registered
  // with the framework's LightShow, so the lighting engine leaves them be.
  FastLED.addLeds<WS2812B, STATUS_LED_PIN, GRB>(statusLeds, NUM_PORTS);
  FastLED.setBrightness(STATUS_LED_BRIGHTNESS);
  fill_solid(statusLeds, NUM_PORTS, CRGB::Black);
  FastLED.show();

  loadCalibration();

  bmDevice = new BMDevice(DEVICE_NAME, SERVICE_UUID, FEATURES_UUID, STATUS_UUID);
  bmDevice->setCustomFeatureHandler(handleCustomFeature);

  if (!bmDevice->begin()) {
    Serial.println("✗ BLE init failed");
  }

  // This device has no light show, so replace the inherited lighting status
  // chunks (power/effect/palette/...) with the single battery chunk the app
  // actually reads.
  bmDevice->clearStatusChunks();
  bmDevice->registerStatusChunk(
    "batt", []() { sendBatteryStatusChunk(); }, "Per-port battery voltages and state");

  // Keep the framework powered on so it never runs its blank-the-strip path
  // (the only place it touches FastLED globally), which would fight our LEDs.
  bmDevice->getState().power = true;

  Serial.printf("Monitoring %d ports, calibration %.3f\n", NUM_PORTS, calibrationFactor);
}

void loop() {
  unsigned long now = millis();

  if (now - lastVoltageReadTime >= VOLTAGE_READ_INTERVAL) {
    lastVoltageReadTime = now;

    bool stateChanged = false;
    for (int i = 0; i < NUM_PORTS; i++) {
      float mv = readPortMilliVolts(i);

      // Moving average over the last few medians.
      movingAverageBuffer[i][bufferIndex[i]] = mv;
      bufferIndex[i] = (bufferIndex[i] + 1) % MOVING_AVERAGE_SIZE;
      if (bufferIndex[i] == 0) bufferFilled[i] = true;

      int samples = bufferFilled[i] ? MOVING_AVERAGE_SIZE : bufferIndex[i];
      float avg = 0.0f;
      for (int j = 0; j < samples; j++) avg += movingAverageBuffer[i][j];
      avg /= (samples > 0 ? samples : 1);

      int mvRounded = (int)lroundf(avg);
      PortState state = classifyPort(i, mvRounded);

      latestMilliVolts[i] = mvRounded;
      if (state != latestState[i]) {
        stateChanged = true;
      }
      latestState[i] = state;
    }

    updateStatusLEDs();

    // A state transition (plugged in, reached full, fault) is worth pushing to
    // the app right away. Live voltage drift is left to the app's poll while its
    // page is open, so a stable box is quiet on the radio.
    if (stateChanged && bmDevice) {
      bmDevice->markStatusDirty();
    }
  }

  // Blink faulted ports without waiting for the next read pass.
  if (now - lastFaultBlink >= FAULT_BLINK_INTERVAL) {
    lastFaultBlink = now;
    bool anyFault = false;
    for (int i = 0; i < NUM_PORTS; i++) {
      if (latestState[i] == PS_FAULT) anyFault = true;
    }
    if (anyFault) {
      faultBlinkOn = !faultBlinkOn;
      updateStatusLEDs();
    }
  }

  if (bmDevice) {
    bmDevice->loop();
  }
}

// Median of NUM_READINGS calibrated samples, scaled through the divider to the
// battery voltage in millivolts. analogReadMilliVolts() applies the chip's ADC
// calibration per sample; the median rejects switching-noise spikes. No
// inter-sample delay: the reads are fast enough to stay clear of the BLE poll.
float readPortMilliVolts(int portIndex) {
  uint8_t pin = PORTS[portIndex].adcPin;
  // One throwaway conversion so the ADC mux/sample-hold settles after switching
  // pins - the first read on a freshly selected channel is often low.
  (void)analogReadMilliVolts(pin);

  int readings[NUM_READINGS];  // pin millivolts, already calibrated
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = analogReadMilliVolts(pin);
  }
  // Insertion sort — small, fixed N.
  for (int i = 1; i < NUM_READINGS; i++) {
    int key = readings[i];
    int j = i - 1;
    while (j >= 0 && readings[j] > key) {
      readings[j + 1] = readings[j];
      j--;
    }
    readings[j + 1] = key;
  }
  int pinMilliVolts = readings[NUM_READINGS / 2];  // median, calibrated
  return pinMilliVolts * VOLTAGE_DIVIDER_RATIO * calibrationFactor;  // battery mV
}

PortState classifyPort(int portIndex, int milliVolts) {
  if (milliVolts >= MV_FAULT) return PS_FAULT;              // genuine over-voltage
  if (milliVolts >= MV_EMPTY_FLOAT || milliVolts < MV_ABSENT_LOW) {
    reachedFull[portIndex] = false;                          // no battery on this port
    return PS_NONE;
  }
  // A battery is present. Latch "reached full" so a finished battery keeps
  // reading FULL as it relaxes back down, rather than flipping to charging.
  if (milliVolts >= MV_FULL_MARK) reachedFull[portIndex] = true;
  return reachedFull[portIndex] ? PS_FULL : PS_CHARGING;
}

void updateStatusLEDs() {
  for (int i = 0; i < NUM_PORTS; i++) {
    CRGB color;
    switch (latestState[i]) {
      case PS_NONE:     color = CRGB::Black;              break;
      case PS_CHARGING: color = CRGB(255, 70, 0);         break;  // amber
      case PS_FULL:     color = CRGB(0, 200, 0);          break;  // green
      case PS_FAULT:    color = faultBlinkOn ? CRGB(255, 0, 0)
                                             : CRGB::Black; break; // blinking red
    }
    statusLeds[PORTS[i].ledIndex] = color;
  }
  FastLED.show();
}

// One compact JSON chunk for the whole box. `mv` and `st` are parallel arrays
// in PORTS order; the app pairs them with its own labels. This stays well under
// the BLE notify ceiling for 8 ports.
void sendBatteryStatusChunk() {
  String s = "{\"type\":\"batt\",\"p\":";
  s += NUM_PORTS;
  s += ",\"cal\":";
  s += String(calibrationFactor, 3);
  s += ",\"lbl\":[";
  for (int i = 0; i < NUM_PORTS; i++) {
    if (i > 0) s += ",";
    s += "\"";
    s += PORTS[i].label;
    s += "\"";
  }
  s += "],\"mv\":[";
  for (int i = 0; i < NUM_PORTS; i++) {
    if (i > 0) s += ",";
    s += latestMilliVolts[i];
  }
  s += "],\"st\":[";
  for (int i = 0; i < NUM_PORTS; i++) {
    if (i > 0) s += ",";
    s += (int)latestState[i];
  }
  s += "]}";
  bmDevice->getBluetoothHandler().sendStatusUpdate(s);
}

bool handleCustomFeature(uint8_t feature, const uint8_t* data, size_t length) {
  switch (feature) {
    case BLE_FEATURE_GET_BATTERY:
      // Answer the app's on-demand request immediately.
      if (bmDevice) bmDevice->markStatusDirty();
      return true;

    case BLE_FEATURE_SET_CALIBRATION:
      // The framework hands the custom handler the whole write, feature byte and
      // all, so the float payload starts at data[1] and a 4-byte float makes the
      // length 5.
      if (length >= 5) {
        float value;
        memcpy(&value, data + 1, sizeof(float));
        if (value > 0.5f && value < 2.0f) {  // sanity band
          saveCalibration(value);
          Serial.printf("[BLE] calibration set to %.3f\n", value);
          if (bmDevice) bmDevice->markStatusDirty();
        }
      }
      return true;

    case BLE_FEATURE_RESET_CALIBRATION:
      saveCalibration(DEFAULT_CALIBRATION);
      Serial.println("[BLE] calibration reset");
      if (bmDevice) bmDevice->markStatusDirty();
      return true;

    case BLE_FEATURE_RESCAN_PORTS:
      // Forget the "reached full" latches so every port re-evaluates charging vs
      // full from its live voltage. Empty ports and removed batteries already
      // detect themselves from the float voltage, so this is just a manual
      // recheck of the charging/full split.
      for (int i = 0; i < NUM_PORTS; i++) reachedFull[i] = false;
      Serial.println("[BLE] full latches cleared");
      if (bmDevice) bmDevice->markStatusDirty();
      return true;

    default:
      return false;  // let the framework handle owner/name/request_status/etc.
  }
}

void loadCalibration() {
  calPrefs.begin("battcal", true);  // read-only
  calibrationFactor = calPrefs.getFloat("cal", DEFAULT_CALIBRATION);
  calPrefs.end();
}

void saveCalibration(float value) {
  calibrationFactor = value;
  calPrefs.begin("battcal", false);  // read-write
  calPrefs.putFloat("cal", value);
  calPrefs.end();
}
