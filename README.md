# Radiation Detector

A digital radiation monitoring system built on the ESP32-S3 platform with real-time measurement, visualization and wireless access.

![Radiation Detector](images/schematic.png)

## Table of Contents
- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Software Architecture](#software-architecture)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Technical Details](#technical-details)
- [OTA Updates](#ota-updates)
- [Development](#development)
- [Troubleshooting](#troubleshooting)
- [License](#license)
- [Acknowledgments](#acknowledgments)
- [Safety Warning](#safety-warning)

## Overview

This project combines an ESP32-S3 microcontroller with a Geiger counter to create a versatile radiation monitoring system. It features a touchscreen interface for local readings, WiFi connectivity for remote monitoring and OTA firmware updates.

The system provides:
- Real-time measurement of radiation levels in µSv/h
- Historical data charts (1‑hour and 24‑hour)
- Customizable alarm thresholds
- Web dashboard for remote monitoring
- OTA firmware updates
- mDNS support (access via `radiation.local`)

## Hardware Requirements
- ESP32-S3 development board
- Geiger counter module (connected to GPIO 9)
- 3.5" TFT display (480x320)
- Buzzer (GPIO 38)
- USB power supply

## Software Architecture

The firmware is built with PlatformIO using the Arduino framework and consists of:
1. **Radiation Measurement** – uses the ESP32 PCNT peripheral
2. **User Interface** – LVGL based touchscreen menus
3. **Data Visualization** – real‑time and historical charts
4. **Web Dashboard** – remote monitoring with OTA
5. **mDNS Support** – `radiation.local` hostname

## Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Libraries installed automatically by PlatformIO:
  - LVGL
  - TFT_eSPI
  - ArduinoJson
  - ElegantOTA
  - ESPmDNS

### Building and Flashing
1. Clone the repository
   ```bash
   git clone https://github.com/username/Radiation-Detector.git
   ```
2. Open the firmware folder with PlatformIO
   ```bash
   cd Radiation-Detector/Firmware/Radiation_Detector
   platformio run --target upload
   ```

## Usage

### Physical Interface
After boot the device displays the main screen with four metrics:
- Current Radiation (µSv/h)
- Average Radiation (µSv/h)
- Maximum Radiation (µSv/h)
- Cumulative Dose (mSv)

Swipe to view charts or open the settings menu for WiFi and alarm configuration.

### Web Interface
Once connected to WiFi the device hosts a dashboard available via its IP or <http://radiation.local>.
The dashboard shows current readings, historical charts and provides OTA update functionality.

## Configuration

### WiFi Setup
1. On the Settings screen enter your WiFi credentials
2. Enable "Connect on Startup" if desired
3. Press **Connect**

### Alarm Settings
1. Open the Settings screen
2. Set the radiation threshold (µSv/h)
3. Set the cumulative dose threshold (mSv)
4. Enable or disable the alarm

## Technical Details

### Radiation Measurement
- Uses the ESP32 PCNT peripheral to count pulses from the Geiger tube
- Samples every 100 ms into 1‑second intervals
- 153.8 pulses/minute ≈ 1 µSv/h (calibrated for the reference tube)
- 60‑second running average to reduce noise

### Data Visualization
- 1‑hour chart: 20 segments of 3‑minute averages
- 24‑hour chart: 24 segments of 1‑hour averages
- Dynamic Y‑axis scaling based on measured values

### Power Management
- Display dims after 5 minutes of inactivity
- Touch to wake the screen
- USB‑powered design (no battery management)

## OTA Updates
The web dashboard allows firmware updates over WiFi:
1. Visit <http://radiation.local>
2. Navigate to the update page
3. Upload the new firmware file

## Development

### Project Structure
- `src/Radiation-Detector.cpp` – main application code
- `src/dashboard.*` – web dashboard implementation
- `src/radiation_data.h` – data exchange interface
- `src/ui.*` – UI components and layout

### Adding Features
1. Modify UI elements in `ui.c` and `ui.h`
2. Update web dashboard in `dashboard.cpp`
3. Add hardware interfaces in `Radiation-Detector.cpp`

## Troubleshooting
- **WiFi Connection Issues** – check credentials or try a manual connection
- **No Radiation Readings** – verify Geiger tube connection on GPIO 9
- **Display Not Working** – confirm TFT wiring and initialization
- **OTA Update Failing** – ensure stable WiFi and correct firmware file

## License

This project is licensed under the MIT License – see individual library folders for details.

## Acknowledgments
- LVGL for the UI framework
- TFT_eSPI for display support
- ElegantOTA for update functionality
- ESPmDNS for local hostname support

## Safety Warning

This device is for educational and hobby use only. It must not be relied on as a primary radiation safety device in hazardous environments. Always use certified equipment for critical applications.
