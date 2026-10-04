# JK-JBD-BMS-Smart-Monitor-ESP32-C6

[![ESP32-C6](https://img.shields.io/badge/MCU-ESP32--C6%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Protocol](https://img.shields.io/badge/Protocols-JK--BMS%20%7C%20JBD--BMS-brightgreen.svg)]()
[![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-orange.svg)](https://platformio.org/)
[![UI](https://img.shields.io/badge/WebUI-Dark%20SPA%20(EN%20%2F%20UA)-purple.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

🌐 **[Українська версія](README.md)** | **English Version**

**Universal, fully autonomous Bluetooth Low Energy (BLE) Gateway and Web Monitor for battery packs (LiFePO4 / Li-ion / LTO) powered by ESP32-C6 (with ESP32-S3 cross-compatibility).**

Simultaneously supports the two most popular BMS protocols in energy storage: **JK-BMS (JiKong)** and **JBD-BMS (Jiabaida / Xiaoxiang / Overkill Solar)** with automatic protocol detection.

---

## ⚡ Key Highlights & Features

* 🔄 **Dual Protocol Support (JK & JBD)**:
  * **JBD-BMS / Xiaoxiang / Overkill Solar**: BLE Service `0xFF00`, queries Basic Info (`0x03`), Cell Voltages (`0x04`), Device Name (`0x05`), verifies 16-bit CRC checksums, and toggles MOSFET switches (`0xE1`).
  * **JK-BMS / JiKong**: BLE Service `0xFFE0`, parses complete ~300-byte TLV frames, supports PIN authentication, and controls Charge MOS, Discharge MOS, and Active Balancer.
  * **Auto-Detect**: Automatically discovers and binds to the connected BMS type based on BLE advertising services and device characteristics.

* 🔋 **100% Autonomous & Standalone**:
  * Runs directly on the ESP32-C6 / ESP32-S3 microcontroller without requiring any external PC, Raspberry Pi, Home Assistant server, or inverter.
  * Powered via standard 5V USB Type-C or an auxiliary DC-DC step-down converter.

* 🖥️ **Modern Dark-Mode Responsive SPA Dashboard**:
  * Mobile-first responsive web interface for smartphones, tablets, and desktops.
  * Instant bilingual UI toggle between **English** and **Ukrainian** (🌐 EN / UA).
  * **Quick KPIs**: Total Pack Voltage, Battery Current (with color-coded Charge/Discharge indicators), Active Power (W), State of Charge (SOC%), Remaining & Nominal Capacity (Ah), Cycle Count.
  * **Cell Histograms (4S / 8S / 16S / 24S)**: 1 mV cell voltage resolution, automatic **MIN** and **MAX** cell highlighting, and real-time voltage Delta calculation ($\Delta V$ in mV).
  * **Protection & Balancing Switches**: Interactive toggle switches for Charge MOS, Discharge MOS, and Active Balancer with optimistic UI updates.
  * **Live Hardware Diagnostics**: Power MOSFET and battery sensor temperatures, Wi-Fi RSSI signal strength, and free internal heap memory.

* 🌐 **Networking & Seamless Setup**:
  * Dual-mode Wi-Fi: Station Mode (home router connection) + Fallback SoftAP (`BMS-Monitor-AP`, `192.168.4.1`) with DNS Captive Portal.
  * Local mDNS hostname: **`http://bms-monitor.local`**.
  * Built-in Wi-Fi and Bluetooth BMS scanners on the `/setup` configuration page.

* 🔘 **Hardware BOOT Button Functions (GPIO 9 on ESP32-C6)**:
  * **Short Click (< 3s)**: Forces an immediate BLE disconnection and fresh handshake without rebooting the web server or Wi-Fi.
  * **Long Press (> 3s)**: Performs a **Factory Reset** (erases saved NVS credentials and restarts into initial Access Point mode).

---

## 📐 Project Architecture

```
esp32c6_universal_bms_monitor/
├── bin/                               # Pre-compiled ready-to-flash binaries
│   ├── esp32c6_universal_bms_monitor_factory.bin   # Complete factory image for ESP32-C6 (Offset 0x0000)
│   ├── esp32c6_universal_bms_monitor_app.bin       # App firmware for ESP32-C6 (Offset 0x10000)
│   ├── esp32s3_universal_bms_monitor_factory.bin   # Complete factory image for ESP32-S3 (Offset 0x0000)
│   └── esp32s3_universal_bms_monitor_app.bin       # App firmware for ESP32-S3 (Offset 0x10000)
├── include/
│   ├── BmsBleClient.h                 # Dual BLE Central client (JK & JBD protocols)
│   ├── BmsProtocol.h                  # Unified telemetry structure (BmsTelemetry)
│   ├── Config.h                       # NVS configuration storage manager
│   └── WebDashboard.h                 # Embedded HTML5/CSS3/JS Dark SPA Dashboard & Setup
├── src/
│   ├── BmsBleClient.cpp               # BLE driver implementation, packet reassembly & CRC
│   └── main.cpp                       # WebServer, Captive Portal, REST API & BOOT button handler
├── platformio.ini                     # Build environments for esp32-c6-supermini & esp32-s3-supermini
├── README.md                          # Ukrainian Documentation
└── README_EN.md                       # English Documentation
```

---

## 📡 REST API Reference

| Method | Endpoint | Description | Payload Example |
|---|---|---|---|
| `GET` | `/` | Main SPA Monitoring Dashboard | — |
| `GET` | `/setup` | Configuration & Scanner Webpage | — |
| `GET` | `/api/data` | Real-time Battery Telemetry (JSON) | — |
| `POST` | `/api/switch` | Toggle MOSFET switches / Balancer | `{"switch": "charging"|"discharging"|"balancer", "state": true|false}` |
| `POST` | `/api/set-cells` | Override active cell count | `{"cells": 8}` |
| `GET` | `/api/scan-wifi` | Scan nearby Wi-Fi networks | — |
| `GET` | `/api/scan-ble` | Discover nearby Bluetooth BMS devices | — |
| `GET` | `/api/config` | Retrieve current saved configuration | — |
| `POST` | `/api/save-config` | Save new Wi-Fi and BMS settings to NVS | `{"ssid":"...","pass":"...","mac":"...","type":0,"pin":"1234","cells":8}` |

---

## 🚀 Quick Start & Flashing Guide

### Option 1: Flashing Pre-built Binary via `esptool`

Connect your ESP32-C6 via USB and run:

```bash
# 1. Erase flash memory (recommended):
esptool.py --port /dev/ttyUSB0 erase_flash

# 2. Flash single all-in-one factory image at offset 0x0000:
esptool.py --port /dev/ttyUSB0 --baud 921600 write_flash 0x0000 bin/esp32c6_universal_bms_monitor_factory.bin
```
*(On macOS, replace `/dev/ttyUSB0` with `/dev/cu.usbmodem...`)*.

---

### Option 2: Building from Source (PlatformIO)

```bash
# Compile for ESP32-C6:
pio run -e esp32-c6-supermini

# Flash to connected board:
pio run -e esp32-c6-supermini -t upload

# Open Serial Monitor:
pio device monitor -b 115200
```

*(For ESP32-S3 boards, use the environment `-e esp32-s3-supermini`)*.

---

## ⚙️ Initial Device Configuration

1. Upon first power-up, the device broadcasts a Wi-Fi Access Point: **`BMS-Monitor-AP`** (open network, no password).
2. Connect from your phone or PC and open **`http://192.168.4.1`** in any web browser.
3. In the setup form:
   * Select your home Wi-Fi network and enter the password.
   * Click **🔍 Find BMS** to scan for nearby devices, or type the Bluetooth MAC manually.
   * Select BMS Type (*Auto Detect*, *JK-BMS*, or *JBD-BMS*) and Cell Configuration (*4S, 8S, 16S, 24S*).
   * Click **💾 Save & Connect**.
4. The ESP32-C6 will restart, join your local network, and become accessible at: **`http://bms-monitor.local`**.

---

## 📄 License

This project is open-source and licensed under the permissive **MIT License**.
Developed for the off-grid and energy-independence community.
