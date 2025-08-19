#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <FastLED.h>

// LED Configuration for basic light show
#define LED_PIN 17  // GPIO 17 for ESP32-C6
#define NUM_LEDS 30
CRGB leds[NUM_LEDS];

// ESP-NOW Configuration
#define DEVICE_OWNER "Cody"
#define DEVICE_TYPE "SyncTest"

uint8_t localMAC[6];
unsigned long messagesSent = 0;
unsigned long messagesReceived = 0;
unsigned long lastDiscovery = 0;
unsigned long lastEffectChange = 0;
int currentEffect = 0;

struct SyncMessage {
    char owner[16];
    char device[32];
    uint8_t messageType;  // 1=discovery, 2=effect_change
    uint8_t effectId;
    uint8_t brightness;
    uint32_t counter;
};

String macToString(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

void onDataReceive(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    messagesReceived++;
    Serial.printf("*** ESP-NOW RECEIVED #%lu ***\n", messagesReceived);
    Serial.printf("From: %s, RSSI: %d\n", macToString(recv_info->src_addr).c_str(), recv_info->rx_ctrl->rssi);
    
    if (len == sizeof(SyncMessage)) {
        SyncMessage msg;
        memcpy(&msg, data, len);
        Serial.printf("Owner: '%.15s', Device: '%.31s'\n", msg.owner, msg.device);
        Serial.printf("Type: %d, Effect: %d, Brightness: %d\n", msg.messageType, msg.effectId, msg.brightness);
        
        // If this is from our owner group and it's an effect change
        if (strcmp(msg.owner, DEVICE_OWNER) == 0 && msg.messageType == 2) {
            Serial.println("✅ APPLYING SYNC UPDATE");
            currentEffect = msg.effectId;
            FastLED.setBrightness(msg.brightness);
            
            // Simple effect switching
            if (currentEffect == 0) {
                fill_solid(leds, NUM_LEDS, CRGB::Red);
            } else if (currentEffect == 1) {
                fill_solid(leds, NUM_LEDS, CRGB::Green);
            } else {
                fill_solid(leds, NUM_LEDS, CRGB::Blue);
            }
            FastLED.show();
        }
    }
    Serial.println("================================");
}

void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    messagesSent++;
    Serial.printf("ESP-NOW SENT #%lu to %s: %s\n", 
                 messagesSent, macToString(mac).c_str(), 
                 status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

void sendDiscovery() {
    SyncMessage msg;
    strncpy(msg.owner, DEVICE_OWNER, sizeof(msg.owner) - 1);
    strncpy(msg.device, DEVICE_TYPE, sizeof(msg.device) - 1);
    msg.messageType = 1; // Discovery
    msg.effectId = currentEffect;
    msg.brightness = 50;
    msg.counter = millis();
    
    uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastMAC, (uint8_t*)&msg, sizeof(msg));
}

void sendEffectChange() {
    SyncMessage msg;
    strncpy(msg.owner, DEVICE_OWNER, sizeof(msg.owner) - 1);
    strncpy(msg.device, DEVICE_TYPE, sizeof(msg.device) - 1);
    msg.messageType = 2; // Effect change
    msg.effectId = currentEffect;
    msg.brightness = 80;
    msg.counter = millis();
    
    uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastMAC, (uint8_t*)&msg, sizeof(msg));
    
    Serial.printf("*** SENT EFFECT CHANGE: Effect %d ***\n", currentEffect);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== ESP-NOW SYNC TEST (NO BLE) ===");
    Serial.println("This test ONLY uses ESP-NOW (no BLE interference)");
    
    // Initialize LEDs
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    
    // Setup WiFi for ESP-NOW ONLY
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(100);
    
    // Set channel FIRST
    esp_err_t channelResult = esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    Serial.printf("WiFi channel set to 6, result: %d\n", channelResult);
    delay(50);
    
    // Get MAC
    esp_wifi_get_mac(WIFI_IF_STA, localMAC);
    Serial.printf("Local MAC: %s\n", macToString(localMAC).c_str());
    Serial.printf("WiFi Channel: %d\n", WiFi.channel());
    
    // Initialize ESP-NOW
    esp_err_t initResult = esp_now_init();
    if (initResult != ESP_OK) {
        Serial.printf("ESP-NOW init failed: %d\n", initResult);
        return;
    }
    Serial.println("ESP-NOW initialized successfully");
    
    // Register callbacks
    esp_now_register_recv_cb(onDataReceive);
    esp_now_register_send_cb(onDataSent);
    
    // Add broadcast peer
    uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMAC, 6);
    peerInfo.channel = 0;  // Use 0 for broadcast
    peerInfo.encrypt = false;
    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    Serial.printf("Broadcast peer add result: %d\n", addResult);
    
    Serial.println("=== TEST READY ===");
    Serial.println("- Sends discovery every 3 seconds");
    Serial.println("- Changes effect every 10 seconds");
    Serial.println("- Watch for messages from other devices");
    Serial.println("- LED shows current effect (Red/Green/Blue)");
}

void loop() {
    // Send discovery every 3 seconds
    if (millis() - lastDiscovery > 3000) {
        Serial.printf("Sending discovery... (Sent: %lu, Received: %lu)\n", messagesSent, messagesReceived);
        sendDiscovery();
        lastDiscovery = millis();
    }
    
    // Change effect every 10 seconds to test sync
    if (millis() - lastEffectChange > 10000) {
        currentEffect = (currentEffect + 1) % 3;
        
        // Update local LED
        if (currentEffect == 0) {
            fill_solid(leds, NUM_LEDS, CRGB::Red);
        } else if (currentEffect == 1) {
            fill_solid(leds, NUM_LEDS, CRGB::Green);
        } else {
            fill_solid(leds, NUM_LEDS, CRGB::Blue);
        }
        FastLED.show();
        
        // Send sync message
        sendEffectChange();
        lastEffectChange = millis();
    }
    
    delay(10);
}

