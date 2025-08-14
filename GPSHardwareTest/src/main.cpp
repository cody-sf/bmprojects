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
  
  // Test different baud rates - expanded list
  int baudRates[] = {9600, 4800, 38400, 19200, 57600, 115200, 14400, 28800, 76800, 230400};
  int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
  
  for (int i = 0; i < numRates; i++) {
    Serial.printf("Testing GPS at %d baud...\n", baudRates[i]);
    gpsSerial.begin(baudRates[i], SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    delay(3000); // Longer delay for GPS data
    
    if (gpsSerial.available() > 0) {
      String data = "";
      int charCount = 0;
      while (gpsSerial.available() > 0 && charCount < 200) {
        char c = gpsSerial.read();
        data += c;
        charCount++;
      }
      
      Serial.printf("Data at %d baud (%d chars): ", baudRates[i], charCount);
      
      // Check if data looks like valid NMEA
      if (data.indexOf("$GP") >= 0 || data.indexOf("$GN") >= 0 || data.indexOf("$GL") >= 0) {
        Serial.println("*** FOUND VALID NMEA! ***");
        Serial.println(data);
        Serial.printf("*** CORRECT BAUD RATE: %d ***\n", baudRates[i]);
        break;
      } else {
        // Show first 50 chars to see what we're getting
        String sample = data.substring(0, 50);
        Serial.printf("Invalid: %s...\n", sample.c_str());
      }
    } else {
      Serial.println("No data received");
    }
  }
  
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