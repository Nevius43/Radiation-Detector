# ESP32-S3 Radiation Detector

A small, open-source handheld **radiation detector/logger** based on the **ESP32-S3**.  
It reads a **Geiger–Müller (GM) tube** and can optionally handle a **scintillation probe (CsI(Tl) + SiPM)**.  
Data is shown on a **4" SPI TFT** and can be logged to **microSD**.

> ⚠️ **Safety:** This project uses **high voltage (≈400–600 V)** and is **not** a certified dosimeter. Build and use at your own risk.

---

## Features
- Live CPS/CPM and rough µSv/h estimate (tube-dependent factor).
- Optional scintillation input through a simple analog chain + discriminator.
- 4" SPI TFT with resistive touch; buzzer clicks/alarms.
- microSD CSV logging (timestamp, cps/cpm, battery, HV, etc.).
- Li-Po power, USB charging, 3.3 V buck; battery voltage monitor.
- Simple config via `src/config.h`. PlatformIO project.

---

## Hardware (summary)
- **MCU:** ESP32-S3-WROOM-1U-N16R8  
- **Display:** ST7796S (SPI) + XPT2046 touch (SPI)  
- **Storage:** microSD (SPI)  
- **GM subsystem:** HV boost (~500 V) + GM input (e.g., Philips 18504)  
- **Optional scintillation:** CsI(Tl) + SiPM (e.g., MicroFC-60035)  
- **Power:** MCP73831 charger, SY8089 buck, battery divider to ADC

Full schematics/PCB are in `/PCB`.

---
