void program1() {
  Serial.println("Program 1");
  getPitch(1);
  getPitch(2);
  
  // Map roll1 from range [-90, 90] to [0, 255]
  int hue = mapValue(roll1, -90, 90, 0, 255);
  
  // Map pitch1 from range [-90, 90] to [0, 255]
  int sat = mapValue(pitch1, -90, 90, 0, 255);
  
  light_show.setCHSV(hue, sat, 255);
  Serial.println("Program 1");
  delay(50);
}