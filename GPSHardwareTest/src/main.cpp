#include <HardwareSerial.h>

// 17, 16 for Vinny/Ashton
// 16, 17 for everyone else

#define GPS_RX_PIN 21  // Replace with the correct RX pin
#define GPS_TX_PIN 22  // Replace with the correct TX pin

HardwareSerial gpsSerial(2);  // Use UART2 for the GPS

unsigned long lastPrintTime = 0;
String gpsBuffer = "";
bool dataReceived = false;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Ensure baud rate matches your GPS module
  Serial.println("GPS Hardware Test Started");
  Serial.println("Expecting data every second...");
}

void loop() {
  // Read any available GPS data
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gpsBuffer += c;
    dataReceived = true;
    
    // If we hit a newline, we have a complete NMEA sentence
    if (c == '\n') {
      // We'll print this in the timed section
    }
  }
  
  // Print status every second
  unsigned long currentTime = millis();
  if (currentTime - lastPrintTime >= 1000) {
    lastPrintTime = currentTime;
    
    if (dataReceived && gpsBuffer.length() > 0) {
      Serial.print("GPS Data: ");
      Serial.print(gpsBuffer);
      if (!gpsBuffer.endsWith("\n")) {
        Serial.println();
      }
    } else {
      Serial.println("No GPS data received in the last second");
    }
    
    // Reset for next second
    gpsBuffer = "";
    dataReceived = false;
  }
} 