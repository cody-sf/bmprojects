# GPS Hardware Test

A simple PlatformIO project for testing GPS hardware connectivity and raw data output.

## Description

This project is designed to test GPS module hardware connections by reading and outputting raw GPS data to the Serial Monitor. It uses UART2 on the ESP32 to communicate with the GPS module.

## Pin Configuration

- GPS RX Pin: 21
- GPS TX Pin: 22

**Note:** Pin configuration may vary:
- 17, 16 for Vinny/Ashton
- 16, 17 for everyone else

## Usage

1. Connect your GPS module to the specified pins
2. Upload the firmware to your ESP32
3. Open the Serial Monitor at 115200 baud
4. Raw GPS data will be displayed in the Serial Monitor

## Hardware Requirements

- ESP32 development board
- GPS module
- Connecting wires

## Software Requirements

- PlatformIO
- ESP32 platform package 