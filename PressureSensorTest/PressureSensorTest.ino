const int sensorPin = 34; // ADC pin connected to sensor
const float VoutMin = 0.15125; // Voltage at 0 PSI
const float VoutMax = 2.43; // Voltage at 60 PSI
const float pressureMax = 60.0; // Max pressure in PSI
const int adcResolution = 4095; // ADC resolution for ESP32

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // Set ADC resolution (0-4095)
}

void loop() {
  int adcValue = analogRead(sensorPin); // Read ADC value
  float voltage = (adcValue / float(adcResolution)) * 3.3; // Convert ADC to voltage

  // Map voltage to pressure
  float pressure = ((voltage - VoutMin) / (VoutMax - VoutMin)) * pressureMax;

  // Ensure no negative pressure values
  pressure = max(0.0f, pressure);

  // Print results
  Serial.print("ADC Value: ");
  Serial.print(adcValue);
  Serial.print(", Voltage: ");
  Serial.print(voltage, 3); // Print voltage with 3 decimal places
  Serial.print(" V, Pressure: ");
  Serial.print(pressure, 2); // Print pressure with 2 decimal places
  Serial.println(" PSI");

  delay(500); // Adjust delay as needed
}