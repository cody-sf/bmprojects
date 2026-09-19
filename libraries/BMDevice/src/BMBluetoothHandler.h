#ifndef BM_BLUETOOTH_HANDLER_H
#define BM_BLUETOOTH_HANDLER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include <functional>

#include "BMDeviceLog.h"

// BLE Feature Constants (from your existing code)
#define BLE_FEATURE_POWER 0x01
#define BLE_FEATURE_REQUEST_STATUS 0x02
#define BLE_FEATURE_BRIGHTNESS 0x04
#define BLE_FEATURE_SPEED 0x05
#define BLE_FEATURE_DIRECTION 0x06
#define BLE_FEATURE_ORIGIN 0x07
#define BLE_FEATURE_PALETTE 0x08
#define BLE_FEATURE_SPEEDOMETER 0x09
#define BLE_FEATURE_EFFECT 0x0A
#define BLE_FEATURE_WAVE_WIDTH 0x0B
#define BLE_FEATURE_METEOR_COUNT 0x0C
#define BLE_FEATURE_TRAIL_LENGTH 0x0D
#define BLE_FEATURE_HEAT_VARIANCE 0x0E
#define BLE_FEATURE_MIRROR_COUNT 0x0F
#define BLE_FEATURE_COMET_COUNT 0x10
#define BLE_FEATURE_DROP_RATE 0x11
#define BLE_FEATURE_CLOUD_SCALE 0x12
#define BLE_FEATURE_BLOB_COUNT 0x13
#define BLE_FEATURE_WAVE_COUNT 0x14
#define BLE_FEATURE_FLASH_INTENSITY 0x15
#define BLE_FEATURE_FLASH_FREQUENCY 0x16
#define BLE_FEATURE_EXPLOSION_SIZE 0x17
#define BLE_FEATURE_SPIRAL_ARMS 0x18
#define BLE_FEATURE_COLOR 0x19

// Defaults Management Features
#define BLE_FEATURE_GET_DEFAULTS 0x1A
#define BLE_FEATURE_SET_DEFAULTS 0x1B
#define BLE_FEATURE_SAVE_CURRENT_AS_DEFAULTS 0x1C
#define BLE_FEATURE_RESET_TO_FACTORY 0x1D
#define BLE_FEATURE_SET_MAX_BRIGHTNESS 0x1E
#define BLE_FEATURE_SET_DEVICE_OWNER 0x1F
#define BLE_FEATURE_SET_AUTO_ON 0x20

// GPS Speed Configuration Features
#define BLE_FEATURE_SET_GPS_LOW_SPEED 0x21
#define BLE_FEATURE_SET_GPS_TOP_SPEED 0x22
#define BLE_FEATURE_SET_GPS_LIGHTSHOW_SPEED_ENABLED 0x23
#define BLE_FEATURE_SET_SYNC_ENABLED 0x24

// Generic Device Configuration Features  
#define BLE_FEATURE_SET_OWNER 0x30
#define BLE_FEATURE_SET_DEVICE_TYPE 0x31
#define BLE_FEATURE_CONFIGURE_LED_STRIP 0x32
#define BLE_FEATURE_GET_CONFIGURATION 0x33
#define BLE_FEATURE_RESET_TO_DEFAULTS 0x34
// OTA WiFi + update control. These sit at 0x78+ rather than the old 0x35-0x37:
// the mobile app treats every BMDevice command as "common" and checks that
// table before a device's own, so a code shared with the boofer/umbrella/
// stoplight tables (which use 0x35-0x6A) would hijack that device's command.
// 0x78+ is clear of all of them and of the hotel-sign codes (0x70-0x77).
#define BLE_FEATURE_SET_WIFI_SSID 0x78
#define BLE_FEATURE_SET_WIFI_PASSWORD 0x79
#define BLE_FEATURE_GET_WIFI_STATUS 0x7A
#define BLE_FEATURE_OTA_CHECK_NOW 0x7B
// Custom palettes. The phone owns the library and samples each gradient down to
// CUSTOM_PALETTE_ENTRIES colours before sending it, so the device stores colours
// rather than gradient stops and never interpolates. 0x7C+ for the same reason
// as the OTA codes above.
//   set:    [0x7C][slot][nameLen][name ASCII][CUSTOM_PALETTE_ENTRIES * RGB]
//   delete: [0x7D][slot]
// Selection reuses BLE_FEATURE_PALETTE with the name "custom1".."custom4".
#define BLE_FEATURE_SET_CUSTOM_PALETTE 0x7C
#define BLE_FEATURE_DELETE_CUSTOM_PALETTE 0x7D

// Find-me: strobe white for a few seconds so a lost light can flag itself
// down, then put the show back. Works even while the light is powered off.
// At 0x70+ like every new common code, clear of the per-device tables.
#define BLE_FEATURE_IDENTIFY 0x7E
// Per-strip LED grouping: [0x7F][stripIndex][ledsPerPixel]. The index is the
// order strips were registered with the show (what the "strips" status chunk
// lists), so it works on sketch-configured rigs like the bike, not just
// NVRAM-configured ones.
#define BLE_FEATURE_SET_STRIP_GROUP 0x7F
// Matrix display content (the bike's front-rack 8x32). Both persist in NVRAM
// and survive reboots; devices without a matrix grid store them and simply
// never show them (the display overlay needs a grid to engage).
//   text:   [0x80][ASCII], truncated to MARQUEE_TEXT_MAX - what the marquee scrolls
//   bitmap: [0x81][w][h][MATRIX_BITMAP_COLORS * RGB][ceil(w*h/2) packed 4bpp
//           pixels, high nibble first, row-major from the TOP row] - one write
//           (179 bytes for 32x8)
//   clear:  [0x82]
#define BLE_FEATURE_SET_MARQUEE_TEXT 0x80
#define BLE_FEATURE_SET_MATRIX_BITMAP 0x81
#define BLE_FEATURE_CLEAR_MATRIX_BITMAP 0x82
// Wiring diagnostic: paint every strip as a rainbow along the raw LED chain
// (hue = chain index) for 30 s, bypassing maps, render orders and grids. One
// photo of the matrix identifies how the chain actually snakes - the layouts
// and their tells are documented in scripts/generate_matrix_map.py.
#define BLE_FEATURE_MATRIX_TEST 0x83
// Text rendering style for the matrix display: [0x84][style] (int32 LE from
// the app; the first payload byte is the value). 0 normal, 1 bold. Persisted.
#define BLE_FEATURE_SET_TEXT_STYLE 0x84
// What the marquee glyphs are filled with: [0x85][fill]. 0 the sliding palette
// gradient, 1 fire, 2 rain, 3 plasma. Applies to the marquee and word zoom.
// Persisted.
#define BLE_FEATURE_SET_TEXT_FILL 0x85
// Animation frames for the display's animation mode. Each frame is the bitmap
// format behind a slot index; the phone clears first, then uploads frames
// 0..N-1, because playback runs the contiguous run of set slots from 0.
//   frame: [0x86][slot][w][h][MATRIX_BITMAP_COLORS * RGB][packed 4bpp pixels]
//   clear: [0x87]
#define BLE_FEATURE_SET_ANIM_FRAME 0x86
#define BLE_FEATURE_CLEAR_ANIM_FRAMES 0x87
// Per-strip brightness ceiling: [0x88][stripIndex][max 1-255], indexed like
// 0x7F by registration order. The strand's output is the master brightness
// scaled by max/255 - dim accents next to bright strips, or a strand held
// under the level where it starts to glitch - and the whole rig still dims
// together on the one master slider. Persisted; 255 = uncapped.
#define BLE_FEATURE_SET_STRIP_BRIGHTNESS 0x88
// What the matrix display overlay shows: [0x89][mode] (int32 LE from the app;
// the first payload byte is the value). 0 off (the running effect owns the
// panel again), 1 marquee text, 2 word zoom, 3 the stored bitmap, 4 the
// stored animation. The display is NOT an effect: it owns only the grid
// strip(s) and the rest of the rig keeps playing the selected effect, so
// neither the mode list, the palette, nor group sync are touched by it.
// Persisted; reported in the matrix chunk as `disp`.
#define BLE_FEATURE_SET_MATRIX_DISPLAY 0x89
// The display's own pace in ms: [0x8A][ms int32 LE]. Exactly ms per animation
// frame (the app sends a GIF's native frame time so its rhythm survives);
// marquee scroll and word zoom scale off the same value, 100 = the old speed
// knob's midpoint feel. Clamped to 20-2000. Persisted; reported as `mtxMs`.
// The effect speed knob no longer paces the display at all.
#define BLE_FEATURE_SET_MATRIX_SPEED 0x8A
// The name a person gives this device. Persisted, reported in status, and
// folded into the advertised name so both apps can read it without a pairing
// list of their own.
#define BLE_FEATURE_SET_DEVICE_NAME 0x38

// How many centrals may be attached at once (phone + watch, plus a spare).
// Three is the controller's ceiling, not ours: CONFIG_BTDM_CTRL_BLE_MAX_CONN on
// the classic ESP32, CONFIG_BT_LE_MAX_CONNECTIONS on the C6. ArduinoBLE's ATT
// layer tracks up to ATT_MAX_PEERS (8), so it is never the binding limit.
#define BM_MAX_BLE_CENTRALS 3

class BMBluetoothHandler {
public:
    BMBluetoothHandler(const char* deviceName, const char* serviceUUID, 
                       const char* featuresUUID,
                       const char* statusUUID);
    
    // Initialization
    bool begin();
    void poll();
    
    // Callback registration
    void setFeatureCallback(std::function<void(uint8_t feature, const uint8_t* data, size_t length)> callback);
    void setConnectionCallback(std::function<void(bool connected)> callback);
    
    // Status updates
    void sendStatusUpdate(const String& status);
    
    // Device name management
    void setDeviceName(const char* deviceName);
    
    // Connection state
    bool isConnected() const { return deviceConnected_; }
    uint8_t connectedCentralCount() const { return connectedCount_; }
    // True once the central has enabled notifications on the status characteristic.
    // Sending before this point is wasted work - the central never sees it.
    bool isSubscribed();
    unsigned long getLastSyncTime() const { return lastBluetoothSync_; }
    void updateSyncTime() { lastBluetoothSync_ = millis(); }

    // Advertising control (for ESP-NOW coexistence)
    void startAdvertising();
    void stopAdvertising();
    
private:
    // BLE objects
    BLEService* service_;
    BLECharacteristic* featuresCharacteristic_;
    BLECharacteristic* statusCharacteristic_;
    
    // Device info
    String deviceName_;
    String serviceUUID_;
    String featuresUUID_;
    String statusUUID_;
    
    // Connection state
    bool deviceConnected_;
    // ArduinoBLE reports connect/disconnect per link but keeps no peer count, so
    // track it here: with the phone and the watch both attached, one dropping
    // must not read as "disconnected" and silence status updates to the other.
    uint8_t connectedCount_;
    // Set from a BLE event callback, acted on in poll() - see poll() for why.
    volatile bool advertisingRestartPending_;
    bool initialized_;
    unsigned long lastBluetoothSync_;
    
    // Callbacks
    std::function<void(uint8_t, const uint8_t*, size_t)> featureCallback_;
    std::function<void(bool)> connectionCallback_;
    
    // Static callbacks for BLE events
    static void onBLEConnected(BLEDevice central);
    static void onBLEDisconnected(BLEDevice central);
    static void onFeatureWritten(BLEDevice central, BLECharacteristic characteristic);
    
    // Static instance pointer for callbacks
    static BMBluetoothHandler* instance_;
    
    // Helper methods
    void processFeatureData(const uint8_t* buffer, size_t length);
};

#endif // BM_BLUETOOTH_HANDLER_H 