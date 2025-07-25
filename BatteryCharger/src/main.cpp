#include <Arduino.h>
#include <BMDevice.h>

// Pin definitions - 8 channels for battery monitoring
const int VOLTAGE_PINS[] = {32, 33, 34, 35, 36, 39, 25, 26};
const int NUM_VOLTAGE_PINS = sizeof(VOLTAGE_PINS) / sizeof(VOLTAGE_PINS[0]);

// Bluetooth device configuration
#define DEVICE_NAME "BatteryCharger"
#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define FEATURES_UUID "12345678-1234-1234-1234-123456789abd"
#define STATUS_UUID "12345678-1234-1234-1234-123456789abe"

// Custom feature commands for battery charger
#define BLE_FEATURE_GET_BATTERY_VOLTAGE 0x50
#define BLE_FEATURE_SET_CALIBRATION 0x51
#define BLE_FEATURE_RESET_CALIBRATION 0x52

// ADC configuration
const float ADC_REF_VOLTAGE = 3.3;  // Reference voltage for ESP32
const int ADC_RESOLUTION = 4095;    // 12-bit ADC resolution

// Voltage divider configuration
const float VOLTAGE_DIVIDER_RATIO = 2.0;  // Battery voltage is divided by 2 (4.2V -> 2.1V)

// Calibration factor (adjust this to match your multimeter)
// If multimeter reads 3.95V but ESP32 shows 3.68V, use: 3.95/3.68 = 1.073
const float CALIBRATION_FACTOR = 1.073;  // Adjust this value to match your multimeter

// Averaging configuration
const int NUM_READINGS = 20;  // Number of readings to average

// Advanced filtering configuration
const int MOVING_AVERAGE_SIZE = 5;  // Keep last 5 measurements for moving average
float movingAverageBuffer[NUM_VOLTAGE_PINS][MOVING_AVERAGE_SIZE];
int bufferIndex[NUM_VOLTAGE_PINS];
bool bufferFilled[NUM_VOLTAGE_PINS];

// BMDevice instance
BMDevice* bmDevice = nullptr;

// Global variables for latest readings (accessible by BLE) - arrays for 8 pins
float latestBatteryVoltage[NUM_VOLTAGE_PINS];
float latestPinVoltage[NUM_VOLTAGE_PINS];
float latestRawValue[NUM_VOLTAGE_PINS];
float latestNoiseLevel[NUM_VOLTAGE_PINS];
String latestStatus[NUM_VOLTAGE_PINS];
unsigned long lastVoltageUpdate = 0;

// Non-blocking timing configuration
const unsigned long VOLTAGE_READ_INTERVAL = 1000;  // Read voltages every 1000ms
unsigned long lastVoltageReadTime = 0;

// Loop performance monitoring
unsigned long loopCounter = 0;
unsigned long lastLoopCountReport = 0;
const unsigned long LOOP_COUNT_REPORT_INTERVAL = 5000;  // Report every 5 seconds

// Forward declarations
bool handleCustomFeature(uint8_t feature, const uint8_t* data, size_t length);
void handleConnectionChange(bool connected);
float getMedian(int readings[], int size);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Battery Charger - Voltage Monitor with Bluetooth");
  Serial.println("===============================================");
  
  // Configure all voltage pins explicitly (no pull-ups on ADC pins)
  for (int i = 0; i < NUM_VOLTAGE_PINS; i++) {
    pinMode(VOLTAGE_PINS[i], INPUT);
    Serial.printf("Configured pin %d for voltage monitoring\n", VOLTAGE_PINS[i]);
  }
  
  // Configure ADC
  analogReadResolution(12);  // Set ADC resolution to 12 bits
  
  // Initialize moving average buffers and status for all pins
  for (int pin = 0; pin < NUM_VOLTAGE_PINS; pin++) {
    bufferIndex[pin] = 0;
    bufferFilled[pin] = false;
    latestBatteryVoltage[pin] = 0.0;
    latestPinVoltage[pin] = 0.0;
    latestRawValue[pin] = 0.0;
    latestNoiseLevel[pin] = 0.0;
    latestStatus[pin] = "Initializing";
    
    for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
      movingAverageBuffer[pin][i] = 0.0;
    }
  }
  
  // Initialize BMDevice with Bluetooth
  Serial.println("Initializing Bluetooth...");
  bmDevice = new BMDevice(DEVICE_NAME, SERVICE_UUID, FEATURES_UUID, STATUS_UUID);
  
  // Set custom feature handler for battery-specific commands
  bmDevice->setCustomFeatureHandler(handleCustomFeature);
  bmDevice->setCustomConnectionHandler(handleConnectionChange);
  
  // Register status chunks for periodic battery updates
  bmDevice->registerStatusChunk("battery_summary", []() {
    String batteryData = "";
    for (int i = 0; i < NUM_VOLTAGE_PINS; i++) {
      if (i > 0) batteryData += "|";
      batteryData += "P" + String(VOLTAGE_PINS[i]) + ":" + String(latestBatteryVoltage[i], 2) + "V";
    }
    bmDevice->getBluetoothHandler().sendStatusUpdate("BATTERY_SUMMARY:" + batteryData);
  }, "All battery voltages summary");
  
  bmDevice->registerStatusChunk("noise_levels", []() {
    String noiseData = "";
    for (int i = 0; i < NUM_VOLTAGE_PINS; i++) {
      if (i > 0) noiseData += "|";
      noiseData += "P" + String(VOLTAGE_PINS[i]) + ":" + String(latestNoiseLevel[i], 3) + "V";
    }
    bmDevice->getBluetoothHandler().sendStatusUpdate("NOISE_LEVELS:" + noiseData);
  }, "Noise levels for all pins");
  
  // Start BMDevice (initializes Bluetooth)
  if (bmDevice->begin()) {
    Serial.println("✓ Bluetooth initialized successfully");
    Serial.println("Device name: " DEVICE_NAME);
    Serial.println("Ready for BLE connections!");
  } else {
    Serial.println("✗ Bluetooth initialization failed");
  }
  
  // Test if pin is floating
  Serial.println("");
  Serial.println("Advanced filtering enabled:");
  Serial.println("- Median filter (removes outliers from 20 readings)");
  Serial.println("- Moving average (smooths over last 5 measurements)");
  Serial.println("- Non-blocking operation for responsive Bluetooth");
  Serial.printf("- Voltage readings every %lums\n", VOLTAGE_READ_INTERVAL);
  Serial.println("- Bluetooth status updates every 5 seconds");
  Serial.println("");
  Serial.println("If you see 4095 constantly, check your connections!");
  Serial.printf("Monitoring pins: ");
  for (int i = 0; i < NUM_VOLTAGE_PINS; i++) {
    if (i > 0) Serial.print(", ");
    Serial.print(VOLTAGE_PINS[i]);
  }
  Serial.println("");
  Serial.println("Each pin should be connected to a voltage divider output.");
  Serial.println("");
  Serial.println("BLE Features available:");
  Serial.println("- 0x50: Get all battery voltages");
  Serial.println("- 0x51: Set calibration factor");
  Serial.println("- 0x52: Reset calibration");
  Serial.println("");
  Serial.printf("Reading battery voltages via voltage dividers on %d pins...\n", NUM_VOLTAGE_PINS);
  Serial.println("Data will be shown in compact format due to multiple channels.");
  Serial.println("");
}

// Custom BLE feature handler for battery charger commands
bool handleCustomFeature(uint8_t feature, const uint8_t* data, size_t length) {
  switch (feature) {
    case BLE_FEATURE_GET_BATTERY_VOLTAGE: {
      // Send current battery voltage readings for all pins
      String voltageData = "";
      for (int i = 0; i < NUM_VOLTAGE_PINS; i++) {
        if (i > 0) voltageData += "|";
        voltageData += "P" + String(VOLTAGE_PINS[i]) + ":" + String(latestBatteryVoltage[i], 3) + "V";
      }
      bmDevice->getBluetoothHandler().sendStatusUpdate("BATTERY_VOLTAGES:" + voltageData);
      Serial.println("[BLE] Sent all battery voltages: " + voltageData);
      return true;
    }
    
    case BLE_FEATURE_SET_CALIBRATION: {
      if (length == 4) {
        // Receive new calibration factor as float
        float newCalibration;
        memcpy(&newCalibration, data, sizeof(float));
        if (newCalibration > 0.5 && newCalibration < 2.0) {  // Sanity check
          // Note: We'd need to make CALIBRATION_FACTOR non-const to modify it
          Serial.printf("[BLE] Calibration update requested: %.3f (not implemented - restart required)\n", newCalibration);
          bmDevice->getBluetoothHandler().sendStatusUpdate("CALIBRATION_UPDATE_REQUIRES_RESTART");
        }
      }
      return true;
    }
    
    case BLE_FEATURE_RESET_CALIBRATION: {
      Serial.println("[BLE] Calibration reset requested (restart required)");
      bmDevice->getBluetoothHandler().sendStatusUpdate("CALIBRATION_RESET_REQUIRES_RESTART");
      return true;
    }
    
    default:
      return false;  // Feature not handled
  }
}

// BLE connection status handler
void handleConnectionChange(bool connected) {
  if (connected) {
    Serial.println("[BLE] Client connected to Battery Charger");
  } else {
    Serial.println("[BLE] Client disconnected from Battery Charger");
  }
}

// Median filter function
float getMedian(int readings[], int size) {
  // Simple bubble sort for small arrays
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        int temp = readings[j];
        readings[j] = readings[j + 1];
        readings[j + 1] = temp;
      }
    }
  }
  return readings[size / 2];  // Return middle value
}

void loop() {
  // Check if it's time to read voltages (non-blocking)
  unsigned long currentTime = millis();
  if (currentTime - lastVoltageReadTime >= VOLTAGE_READ_INTERVAL) {
    Serial.printf("Reading %d pins... ", NUM_VOLTAGE_PINS);
    
    // Process each pin
    for (int pin = 0; pin < NUM_VOLTAGE_PINS; pin++) {
      // Take multiple readings for median filtering
      int readings[NUM_READINGS];
      int minRaw = 4095;
      int maxRaw = 0;
      
      for (int i = 0; i < NUM_READINGS; i++) {
        readings[i] = analogRead(VOLTAGE_PINS[pin]);
        
        // Track min/max for debugging
        if (readings[i] < minRaw) minRaw = readings[i];
        if (readings[i] > maxRaw) maxRaw = readings[i];
        
        delay(2);  // Small delay between ADC readings
      }
      
      // Apply median filter (removes outliers better than average)
      float medianRawValue = getMedian(readings, NUM_READINGS);
      float medianPinVoltage = (medianRawValue / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE;
      float instantBatteryVoltage = medianPinVoltage * VOLTAGE_DIVIDER_RATIO * CALIBRATION_FACTOR;
      
      // Add to moving average buffer for this pin
      movingAverageBuffer[pin][bufferIndex[pin]] = instantBatteryVoltage;
      bufferIndex[pin] = (bufferIndex[pin] + 1) % MOVING_AVERAGE_SIZE;
      if (bufferIndex[pin] == 0) bufferFilled[pin] = true;
      
      // Calculate moving average (smooths over time)
      float movingAverageVoltage = 0;
      int validSamples = bufferFilled[pin] ? MOVING_AVERAGE_SIZE : bufferIndex[pin];
      for (int i = 0; i < validSamples; i++) {
        movingAverageVoltage += movingAverageBuffer[pin][i];
      }
      movingAverageVoltage /= validSamples;
      
      // Determine status based on moving average (most stable)
      String status = "";
      if (medianRawValue >= 4090) {
        status = "MAXED";
      } else if (medianRawValue <= 10) {
        status = "ZERO";
      } else if (movingAverageVoltage >= 3.0 && movingAverageVoltage <= 4.3) {
        status = "OK";
      } else {
        status = "OUT_RANGE";
      }
      
      // Calculate variance for noise analysis
      float variance = ((maxRaw - minRaw) / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO * CALIBRATION_FACTOR;
      
      // Update global variables for BLE access
      latestBatteryVoltage[pin] = movingAverageVoltage;
      latestPinVoltage[pin] = medianPinVoltage;
      latestRawValue[pin] = medianRawValue;
      latestNoiseLevel[pin] = variance / 2;
      latestStatus[pin] = status;
    }
    
    lastVoltageUpdate = millis();
    Serial.printf("Done!\n");
    
    // Print compact results for all pins
    Serial.printf("Pin   | Raw   | Filtered | Noise  | Status\n");
    Serial.printf("------|-------|----------|--------|----------\n");
    
    for (int pin = 0; pin < NUM_VOLTAGE_PINS; pin++) {
      Serial.printf("P%-4d | %5.0f | %6.3fV | ±%.3fV | %s\n", 
                    VOLTAGE_PINS[pin], 
                    latestRawValue[pin], 
                    latestBatteryVoltage[pin],
                    latestNoiseLevel[pin],
                    latestStatus[pin].c_str());
    }
    
    // Show BLE connection status and sync info
    if (bmDevice && bmDevice->getBluetoothHandler().isConnected()) {
      Serial.printf("\n✓ BLE Connected");
      unsigned long timeSinceSync = millis() - bmDevice->getBluetoothHandler().getLastSyncTime();
      Serial.printf(" | Last sync: %lus ago\n", timeSinceSync / 1000);
    } else {
      Serial.printf("\n○ BLE Disconnected\n");
    }
    
    Serial.printf("========================================\n\n");
    
    // Update the timing for next reading
    lastVoltageReadTime = currentTime;
  }
  
  // Always handle BMDevice loop (non-blocking Bluetooth processing)
  if (bmDevice) {
    bmDevice->loop();
  }
  
  // Loop performance monitoring (optional debug info)
  loopCounter++;
  if (currentTime - lastLoopCountReport >= LOOP_COUNT_REPORT_INTERVAL) {
    unsigned long loopsPerSecond = loopCounter * 1000 / LOOP_COUNT_REPORT_INTERVAL;
    Serial.printf("[DEBUG] Main loop running at ~%lu Hz (loops/sec) - Bluetooth responsive!\n", loopsPerSecond);
    loopCounter = 0;
    lastLoopCountReport = currentTime;
  }
  
  // Small delay to prevent overwhelming the CPU, but keep BLE responsive
  delay(10);
}
