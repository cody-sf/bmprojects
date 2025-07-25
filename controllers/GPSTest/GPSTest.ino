#include <HardwareSerial.h>

// 17, 16 for Vinny/Ashton
// 16, 17 for everyone eles

#define GPS_RX_PIN 21  // Replace with the correct RX pin
#define GPS_TX_PIN 22  // Replace with the correct TX pin

HardwareSerial gpsSerial(2);  // Use UART2 for the GPS

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Ensure baud rate matches your GPS module
}

void loop() {
  while (gpsSerial.available() > 0) {
    Serial.write(gpsSerial.read());  // Output the raw GPS data to the Serial Monitor
  }
}