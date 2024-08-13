void program2() {
  getPitch(1);
  getPitch(2);

  // Map roll1 from range [-90, 90] to [0, 255]
  int hueIndex = mapValue(roll1, -90, 90, 0, 255);
  
  // Map pitch1 from range [-90, 90] to [0, 255] for brightness control
  int brightness = mapValue(pitch1, -90, 90, 0, 255);

  // Fetch the palette using getPalette method
  CRGBPalette16 selectedPalette = light_show.getPalette(AvailablePalettes::earth);

  // Fetch the color from the palette
  CRGB color = ColorFromPalette(selectedPalette, hueIndex);

  // Scale the brightness
  //color = color.nscale8(brightness);
  
  // Apply the selected color to the light show using the solid method
  light_show.solid(color);
  controlBrightnessWithGyroAccel();

  delay(50);
}
