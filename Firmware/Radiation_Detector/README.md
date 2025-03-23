# Radiation Detector

A digital radiation monitoring system built on the ESP32-S3 platform with real-time radiation measurement, visualization, data logging, and wireless connectivity.

![Radiation Detector](https://github.com/username/Radiation-Detector/raw/main/docs/images/radiation-detector.jpg)

## Overview

This radiation detector project combines an ESP32-S3 microcontroller with a Geiger counter to create a versatile digital radiation monitoring system. It features a touchscreen TFT display for data visualization, WiFi connectivity for remote monitoring, and OTA (Over-The-Air) update capabilities.

The system provides:

- Real-time measurement of radiation levels in µSv/h
- Historical data visualization with 1-hour and 24-hour charts
- Customizable alarm thresholds for radiation levels
- Web dashboard for remote monitoring
- OTA firmware updates
- mDNS support for easy access via "radiation.local"

## Hardware Requirements

- ESP32-S3 development board
- Geiger counter module (connected to GPIO 9)
- 3.5" TFT display (480x320)
- Buzzer (connected to GPIO 38)
- USB power supply

## Software Architecture

The project is built with PlatformIO and Arduino framework and consists of several key components:

1. **Radiation Measurement** - Uses ESP32's PCNT (Pulse Counter) peripheral to count pulses from the Geiger counter
2. **User Interface** - LVGL-based touchscreen interface with multiple screens
3. **Data Visualization** - Real-time and historical charts showing radiation levels
4. **Web Dashboard** - Remote monitoring capabilities
5. **OTA Updates** - Wireless firmware update functionality
6. **mDNS Support** - Local network hostname resolution (radiation.local)

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) (with VS Code or CLI)
- The following libraries (installed automatically via PlatformIO):
  - LVGL (for UI)
  - TFT_eSPI (for display)
  - ArduinoJson
  - ElegantOTA
  - ESPmDNS

### Building and Flashing

1. Clone the repository:
   ```
   git clone https://github.com/username/Radiation-Detector.git
   ```

2. Open the project in PlatformIO:
   ```
   cd Radiation-Detector
   platformio open
   ```

3. Build and upload to your ESP32-S3:
   ```
   platformio run --target upload
   ```

## Usage

### Physical Interface

The device starts with a startup screen showing the bootup sequence. Once initialized:

1. **Main Screen** - Displays current radiation readings with four key metrics:
   - Current Radiation (µSv/h)
   - Average Radiation (µSv/h)
   - Maximum Radiation (µSv/h)
   - Cumulative Dose (mSv)

2. **Charts Screen** - Provides visual history of radiation levels:
   - 1-hour chart (20 segments of 3-minute averages)
   - 24-hour chart (24 segments of 1-hour averages)

3. **Settings Screen** - Configuration options:
   - WiFi connection setup
   - Alarm threshold settings
   - Display settings

### Web Interface

Once connected to WiFi, the device hosts a web dashboard accessible through:

- IP address (displayed on the screen)
- mDNS hostname: http://radiation.local

The web dashboard provides:
- Current radiation readings
- Historical charts
- OTA update functionality (with safety confirmation)

## Configuration

### WiFi Setup

1. On the Settings screen, enter your WiFi credentials
2. Toggle "Connect on Startup" to enable automatic connection
3. Press "Connect" to initiate connection

### Alarm Settings

1. Navigate to the Settings screen
2. Set threshold for current radiation level (µSv/h)
3. Set threshold for cumulative dose (mSv)
4. Enable/disable alarm function using the checkbox

## Technical Details

### Radiation Measurement

- The system uses an ESP32 PCNT peripheral to count pulses from the Geiger counter
- Pulses are sampled every 100ms and accumulated into 1-second intervals
- Conversion factor: 153.8 pulses/minute = 1 µSv/h (calibrated for specific Geiger tube)
- 60-second running average for current readings to reduce statistical noise

### Data Visualization

- 1-hour chart: 20 segments, each representing a 3-minute average
- 24-hour chart: 24 segments, each representing a 1-hour average
- Dynamic Y-axis scaling based on measured values

### Power Management

- Display dimming after 5 minutes of inactivity
- Touch anywhere to restore display brightness
- USB-powered design (no battery monitoring)

## OTA Updates

The device supports Over-The-Air updates via the web interface:

1. Access the web dashboard at http://radiation.local
2. Navigate to the update page
3. Review and acknowledge the warning
4. Select and upload the new firmware file

## Development

### Project Structure

- `src/Radiation-Detector.cpp` - Main application code
- `src/dashboard.*` - Web dashboard implementation
- `src/radiation_data.h` - Data exchange interface
- `src/ui.*` - UI components and layout

### Adding Features

To extend the project:

1. Modify UI elements in `ui.c` and `ui.h`
2. Update web dashboard in `dashboard.cpp`
3. Add new hardware interfaces in `Radiation-Detector.cpp`

## Troubleshooting

- **WiFi Connection Issues**: Check credentials, try manual connection
- **No Radiation Readings**: Verify Geiger counter connection to GPIO 9
- **Display Not Working**: Check TFT connections and initialization
- **OTA Update Failing**: Ensure stable WiFi connection, verify firmware file

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- LVGL for the UI framework
- TFT_eSPI library for display support
- ElegantOTA for update functionality
- ESPmDNS for local hostname support

## Safety Warning

This device is meant for educational and hobbyist purposes only. It should not be used as a primary radiation safety device in hazardous environments. Always use certified radiation detection equipment for critical safety applications. 