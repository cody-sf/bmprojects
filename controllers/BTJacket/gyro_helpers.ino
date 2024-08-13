void getPitch(int side) {
  sensors_event_t a, g, temp;
  if (side == 1){
    mpu1.getEvent(&a, &g, &temp);
    getAngles(a.acceleration.x, a.acceleration.y, a.acceleration.z, pitch1, roll1);
  }
  if (side == 2){
    mpu2.getEvent(&a, &g, &temp);
    getAngles(a.acceleration.x, a.acceleration.y, a.acceleration.z, pitch2, roll2);
  }
}

void getAngles(double Vx, double Vy, double Vz, double &pitch, double &roll) {
  pitch = atan(Vx/sqrt((Vy*Vy) + (Vz*Vz))) * (180.0/M_PI);
  roll = atan(Vy/sqrt((Vx*Vx) + (Vz*Vz))) * (180.0/M_PI);
}

int mapValue(int x, int in_min, int in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void controlBrightnessWithGyro() {
    sensors_event_t accelData2, gyroData2, tempData2;
    
    // Get the event data for accelerometer, gyro, and temperature
    mpu2.getEvent(&accelData2, &gyroData2, &tempData2);

    // Read the X-axis (You can choose Y or Z if you prefer)
    float gyroX2 = gyroData2.gyro.x;

    // Normalize gyro readings. Assume -250 to 250 as raw range. Adjust based on your needs.
    // This will map the gyroX2 reading from its range to a 0-255 range.
    int brightness = map(gyroX2, -250, 250, 100, 255);

    // Constrain the value between 0 and 255 just to be safe.
    brightness = constrain(brightness, 100, 255);
    Serial.println(brightness);
    // Set brightness
    light_show.brightness(brightness);
}

float mapValueFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void controlBrightnessWithGyroAccel() {
  sensors_event_t accelData2, gyroData2, tempData2;
  mpu2.getEvent(&accelData2, &gyroData2, &tempData2);

    float magnitude = sqrt(pow(accelData2.acceleration.x, 2) + pow(accelData2.acceleration.y, 2) + pow(accelData2.acceleration.z, 2));
    magnitude -= 9.81; // Remove static gravity;
    int brightness = mapValueFloat(magnitude, 0, 40, 50, 255);
    brightness = constrain(brightness, 0, 150);
    light_show.brightness(brightness);
}
