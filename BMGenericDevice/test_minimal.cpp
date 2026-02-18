#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== ESP32 MINIMAL TEST ===");
    Serial.println("If you see this, the ESP32 hardware is OK");
}

void loop() {
    Serial.println("ESP32 is alive!");
    delay(2000);
}




