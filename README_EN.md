# Universal-BMS-Smart-Monitor-ESP32-C6

[![ESP32-C6](https://img.shields.io/badge/MCU-ESP32--C6%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Protocol](https://img.shields.io/badge/Protocols-JK%20%7C%20JBD%20%7C%20Daly%20%7C%20Ant%20%7C%20Redodo-brightgreen.svg)]()
[![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-orange.svg)](https://platformio.org/)
[![UI](https://img.shields.io/badge/WebUI-Dark%20SPA%20(EN%20%2F%20UA)-purple.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

🌐 **[Українська версія](README.md)** | **English Version**

**Universal, fully autonomous Bluetooth Low Energy (BLE) Gateway and Web Monitor for battery packs (LiFePO4 / Li-ion / LTO) powered by ESP32-C6 (with ESP32-S3 cross-compatibility).**

Simultaneously supports 5 leading BMS protocols: **JK-BMS (JiKong)**, **JBD-BMS (Jiabaida / Xiaoxiang / Overkill Solar / PowerQueen)**, **Daly Smart BMS**, **Ant BMS**, and **Redodo / LiTime Monolithic Smart BMS** with automatic protocol detection.

---

## ⚡ Key Highlights & Features

* 🔄 **5-in-1 Universal BMS Protocol Support**:
  * **JBD-BMS / Xiaoxiang / Overkill Solar / PowerQueen**: BLE Service `0xFF00`, queries Basic Info (`0x03`), Cell Voltages (`0x04`), Device Name (`0x05`), verifies 16-bit CRC checksums, and toggles MOSFET switches (`0xE1`).
  * **JK-BMS / JiKong**: BLE Service `0xFFE0`, parses complete ~300-byte TLV frames, supports PIN authentication, and controls Charge MOS, Discharge MOS, and Active Balancer.
  * **Daly Smart BMS**: BLE Service `0xFFF0` (`0xFFF1` Notify / `0xFFF2` Write) and proprietary hardware service `02F00000-...-FE00`, telemetry queries `0x90`–`0x98` (cell voltages, current, SOC, capacity, temperatures, alarms, balancing status), Charge & Discharge MOS toggles (`0xDA`, `0xD9`).
  * **Ant BMS**: BLE Service `0xFFE0` (`0xFFE1` Notify / `0xFFE2` Write), NUS or `0xFFF0`, poll requests `0xDB 0xDB`, decodes 140-byte legacy frame `0xAA 0x55 0xAA 0xFF` (up to 32 cells, current, power, temperatures MOS/T1/T2, SOC, capacity), Charge & Discharge MOS toggles and Balancer switch (`0xA5 0xA5 ...`).
  * **Redodo / LiTime Monolithic Smart BMS**: BLE Service `0xFFE0` (`0xFFE1` Notify / `0xFFE2` Write), poll command `0x00 0x00 0x04 0x01 0x13 0x55 0xAA 0x17`, decodes 105-byte frame `0x00 0x00 0x61 0x01 0x93` (terminal voltage, up to 16 cells, current, cell/MOS/ambient temperatures, protections, balance mask, cycle count).
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
  * **Long Press (> 3s)**: Performs a **Factory Reset** (erases saved Wi-Fi & BMS credentials and restarts into initial Access Point mode, preserving Telegram credentials).

* ✈️ **Embedded Telegram Bot Client (Remote Access without Tailscale / Public IP)** *(New in v1.4.0)*:
  * **Tailscale-Free Remote Monitoring**: Operates via outbound HTTPS requests to the Telegram Bot API (`api.telegram.org:443`). Traverses home NAT, mobile cellular networks, and Starlink with zero port forwarding.
  * **Interactive Commands & Menus**:
    * `/status` — Battery overview: Total voltage, current (+/-), active power (W), SOC %, remaining Ah, cycles, cell imbalance $\Delta V$, temperatures, switches, and active balancing status.
    * `/cells` — Detailed cell voltage breakdown (1–32S), minimum `Vmin 🔻`, maximum `Vmax 🔺`, active balancing cells `⚡`, and cell imbalance $\Delta V$ in mV.
    * `/control` — Status of power switches and interactive control buttons.
    * `/info` — ESP32-C6 hardware telemetry (Uptime, free RAM Heap, Wi-Fi RSSI, BLE link status).
  * **BMS Switch Control with Confirmation Guard**:
    * Toggle **Charge MOS**, **Discharge MOS**, and **Active Balancer** directly from Telegram.
    * Mandatory interactive inline confirmation (`⚠️ Are you sure? [YES] [CANCEL]`) eliminates accidental load disconnects.
  * **Proactive Push Alerts on Critical Events**:
    * 🚨 **High Cell Imbalance**: $\Delta V > 100\text{ mV}$ (with auto-normalization alert when imbalance resolves).
    * 🔥 **Overheat Alert**: MOSFET or battery pack temperature $> 55^\circ\text{C}$.
    * 🪫 **Critical Discharge Alert**: $\text{SOC} \le 10\%$ or any cell voltage $< 2.80\text{ V}$.
    * ⚠️ **BLE Link Loss**: Alert triggered if telemetry is interrupted for $> 45$ seconds.
  * **Security Whitelist**: Strictly responds only to authorized User / Chat IDs (supports multiple comma-separated IDs: `12345678, 87654321`).
  * **Non-Blocking & Memory Guarded**: Zero memory allocation when disabled. Telegram credentials remain preserved across BOOT button factory resets (dedicated Web UI button for credential wiping).

---

## 📐 Project Architecture

```
esp32c6_universal_bms_monitor/
├── bin/                               # Pre-compiled ready-to-flash binaries (Release v1.4.0)
│   ├── esp32c6_universal_bms_monitor_factory.bin   # Complete factory image for ESP32-C6 (Offset 0x0000)
│   ├── esp32c6_universal_bms_monitor_app.bin       # App firmware for ESP32-C6 (Offset 0x10000 / OTA)
│   ├── esp32s3_universal_bms_monitor_factory.bin   # Complete factory image for ESP32-S3 (Offset 0x0000)
│   └── esp32s3_universal_bms_monitor_app.bin       # App firmware for ESP32-S3 (Offset 0x10000 / OTA)
├── include/
│   ├── BmsBleClient.h                 # 5-in-1 Universal BLE Central client (JK, JBD, Daly, Ant, Redodo)
│   ├── BmsProtocol.h                  # Normalized telemetry model (BmsTelemetry, 1..32S)
│   ├── Config.h                       # NVS configuration storage manager (Wi-Fi, BMS, Telegram)
│   ├── TelegramBotClient.h            # Non-blocking HTTPS Telegram Bot client with push alerts
│   └── WebDashboard.h                 # Embedded HTML5/CSS3/JS Dark SPA Dashboard & Setup (UA/EN)
├── src/
│   ├── BmsBleClient.cpp               # Multi-protocol parser, auto-detection engine & CRC verification
│   └── main.cpp                       # WebServer, Captive Portal, REST API, BOOT button & FreeRTOS task
├── platformio.ini                     # Build environments for esp32-c6-supermini & esp32-s3-supermini
├── README.md                          # Ukrainian Documentation
└── README_EN.md                       # English Documentation
```

### 🧩 Protocol Engines in `BmsBleClient`:
* **JK-BMS (JiKong)**: BLE Service `0xFFE0` (`0xFFE1` Notify / `0xFFE2` Write), decodes ~300-byte TLV frames, handles PIN handshakes, and controls Charge MOS, Discharge MOS, and Active Balancer registers.
* **JBD-BMS (Jiabaida / Xiaoxiang / Overkill Solar / PowerQueen)**: BLE Service `0xFF00`, queries Basic Info (`0x03`), Cell Voltages (`0x04`), Device Name (`0x05`), validates 16-bit CRC checksums, maps NTC3 (MOSFET temperature) and production date, controls MOSFET switches (`0xE1`).
* **Daly Smart BMS**: BLE Service `0xFFF0` (`0xFFF1`/`0xFFF2`) and hardware service `02F00000-...-FE00`, telemetry queries `0x90`–`0x98`, parses cell voltages, current, SOC, capacity, temperatures, protection flags, controls Charge & Discharge MOS (`0xDA`, `0xD9`).
* **Ant BMS**: BLE Service `0xFFE0` (`0xFFE1`/`0xFFE2`), NUS `6E400001-...` or `0xFFF0`, poll query `0xDB 0xDB`, decodes 140-byte frame `0xAA 0x55 0xAA 0xFF` (up to 32 cells, current, power, MOS Temp, T1, T2, capacity), controls Charge/Discharge MOS and Balancer (`0xA5 0xA5 ...`).
* **Redodo / LiTime Monolithic BMS**: BLE Service `0xFFE0` (`0xFFE1`/`0xFFE2`), poll command `0x00 0x00 0x04 0x01 0x13 0x55 0xAA 0x17`, decodes 105-byte frame `0x00 0x00 0x61 0x01 0x93` (terminal voltage, up to 16 cells, current, cell/MOS/ambient temperatures, protections, balance mask, cycle count).

---

## 📊 BMS Compatibility Matrix

| Brand / Family | Verified Models & Series | Supported Cells | Available Functions in Monitor |
|---|---|---|---|
| **JK (JiKong)** | JK-B1A8S20P, JK-B2A8S20P, JK-B2A16S20P, JK-B2A24S, JK-PB series, JK-BD series | 4S – 24S | Telemetry (1 mV accuracy), Current, SOC, Ah capacity, Cell voltages, Charge MOS, Discharge MOS, Active Balancer toggles |
| **JBD (Jiabaida)** | SP04S034, SP08S004, SP15S001, SP16S001, AP20S003, Overkill Solar 4S/8S, PowerQueen LiFePO4 Smart | 3S – 32S | Telemetry, CRC-16 check, MOSFET Temp (NTC3), Production Date, Charge MOS, Discharge MOS control |
| **Daly Smart BMS** | DL-4S, DL-8S (DL-8S-200A), DL-16S, DL-24S (0xFFF0 modules & FE00 chipset) | 3S – 32S | Telemetry `0x90`–`0x98`, Current, SOC, Ah, Cell voltages, Alarm/Fault flags, Charge & Discharge MOS controls |
| **Ant BMS** | Ant-BMS 8S-20S, 8S-24S, 16S-32S (Bluetooth 4.0 / BLE with 0xAA 0x55 frames) | 7S – 32S | Telemetry 140B, up to 32 cells, 0.1A current, Power, MOS Temp, T1, T2, Charge MOS, Discharge MOS, Balancer switch |
| **Redodo / LiTime** | LiTime 12V 100Ah Smart, LiTime 24V 100Ah Smart, Redodo 12V 100Ah / 200Ah Smart | 4S, 8S, 16S | Telemetry 105B, up to 16 cells, current in mA, cell/MOS/ambient temps, cycles, protection status, balance mask |

---

## 🧪 Testing with BMS Emulator (Universal BMS Emulator)

If you don't have a physical battery pack or BMS at hand, you can test every feature of this monitor using our companion project: [Universal BMS BLE Emulator](https://github.com/tigersumy/JBD-bms-emulator):

1. **Flash an emulator board** (ESP32-C6 or standard ESP32) with the emulator firmware.
2. **Switch emulation mode**:
   * **Via BOOT button at startup**: Hold or double-click the BOOT button to cycle profiles:
     * 🔴 **3 Red flashes** = JBD BMS (`JBD-SP04S034`)
     * 🟣 **3 Magenta flashes** = Daly Smart BMS (`DL-8S-200A`)
     * 🔵 **3 Blue flashes** = Ant BMS (`ANT-BMS-08S`)
     * 🟠 **3 Orange flashes** = JK BMS (`JK-B2A8S20P`)
     * 🟡 **3 Yellow flashes** = Redodo / LiTime Monolithic BMS (`Redodo-12V100A` / `LT-12100B`)
   * **Via Emulator Web Dashboard**: Navigate to `http://192.168.4.1` (or `http://bms-emulator.local`) and select the desired profile from the dropdown menu.
3. **Connect the monitor**:
   * Open the monitor setup page at `http://192.168.4.1/setup` (or `http://bms-monitor.local/setup`).
   * Click **🔍 Find BMS** — the monitor will scan BLE and discover the emulator.
   * Click on the found device — MAC address and protocol type are filled automatically!
   * Click **💾 Save & Connect** to view real-time live telemetry and test switch controls.

---

## 📡 REST API Reference

| Method | Endpoint | Description | Payload Example / Response |
|---|---|---|---|
| `GET` | `/` | Main SPA Monitoring Dashboard | — |
| `GET` | `/setup` | Configuration & Scanner Webpage | — |
| `GET` | `/api/data` | Real-time Battery Telemetry (JSON) | See JSON model below |
| `POST` | `/api/switch` | Toggle MOSFET switches or Active Balancer | `{"switch": "charging"\|"discharging"\|"balancer", "state": true\|false}` |
| `POST` | `/api/set-cells` | Override active cell count | `{"cells": 8}` |
| `GET` | `/api/scan-wifi` | Scan nearby Wi-Fi networks | `[{"ssid":"Home-WiFi","rssi":-58,"secure":true}]` |
| `GET` | `/api/scan-ble` | Discover nearby Bluetooth BMS devices | `[{"name":"JK-B2A8S20P","mac":"AA:BB:..","type":"JK-BMS","rssi":-62}]` |
| `GET` | `/api/config` | Retrieve current saved configuration (including Telegram) | `{"ssid":"...","mac":"...","tg_enabled":true,...}` |
| `POST` | `/api/save-config` | Save Wi-Fi, BMS and Telegram settings to NVS | `{"ssid":"...","mac":"...","tg_enabled":true,...}` |
| `POST` | `/api/telegram-clear` | Explicitly erase saved Telegram token and Chat IDs | `{"status":"ok"}` |
| `POST` | `/api/telegram-test` | Dispatch a test verification message to Telegram | `{"status":"ok"}` |

### 💡 Examples: Controlling switches via `curl`:

```bash
# 1. Enable Charge MOS:
curl -X POST http://bms-monitor.local/api/switch \
     -H "Content-Type: application/json" \
     -d '{"switch": "charging", "state": true}'

# 2. Disable Discharge MOS:
curl -X POST http://bms-monitor.local/api/switch \
     -H "Content-Type: application/json" \
     -d '{"switch": "discharging", "state": false}'

# 3. Enable Active Balancer (for JK-BMS and Ant BMS):
curl -X POST http://bms-monitor.local/api/switch \
     -H "Content-Type: application/json" \
     -d '{"switch": "balancer", "state": true}'
```

---

## 🏠 Home Assistant Integration

### Option 1: RESTful Sensor (Available Now)
Add to your Home Assistant `configuration.yaml`:

```yaml
sensor:
  - platform: rest
    name: "Home Battery BMS"
    resource: "http://bms-monitor.local/api/data"
    scan_interval: 5
    json_attributes:
      - bms_type
      - connected
      - cell_count
      - cell_voltages
      - delta_cell_v
      - min_cell_v
      - max_cell_v
      - temp_mos
      - temp_sensor1
      - temp_sensor2
      - switch_charging
      - switch_discharging
      - switch_balancer
    value_template: "{{ value_json.total_voltage | round(2) }}"
    unit_of_measurement: "V"

  - platform: template
    sensors:
      battery_current:
        friendly_name: "Battery Current"
        unit_of_measurement: "A"
        value_template: "{{ state_attr('sensor.home_battery_bms', 'current') | default(0) }}"
      battery_soc:
        friendly_name: "Battery SOC"
        unit_of_measurement: "%"
        value_template: "{{ state_attr('sensor.home_battery_bms', 'soc') | default(0) }}"
      battery_power:
        friendly_name: "Battery Power"
        unit_of_measurement: "W"
        value_template: "{{ state_attr('sensor.home_battery_bms', 'power') | default(0) }}"
      battery_delta_v:
        friendly_name: "Cell Delta Voltage"
        unit_of_measurement: "V"
        value_template: "{{ state_attr('sensor.home_battery_bms', 'delta_cell_v') | default(0) }}"
```

### Option 2: MQTT Discovery (Roadmap for v1.4.0) 🚀
Planned for **v1.4.0**:
* Native **Home Assistant MQTT Discovery** integration (`homeassistant/sensor/bms_monitor/.../config`).
* Zero configuration: Device and all sensors (Voltages, Current, Power, SOC, Capacity, Temperatures, individual cell 1..32 voltages, and status switches) appear automatically in Home Assistant.
* Two-way switch control directly from Home Assistant UI dashboard cards over MQTT command topics.

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
   * Select BMS Type (*Auto Detect*, *JK-BMS*, *JBD-BMS*, *Daly Smart BMS*, *Ant BMS*, or *Redodo / LiTime*) and Cell Configuration (*4S, 8S, 16S, 24S*).
   * Click **💾 Save & Connect**.
4. The ESP32-C6 will restart, join your local network, and become accessible at: **`http://bms-monitor.local`**.

---

## ✈️ Telegram Bot Setup & Configuration Guide

The embedded Telegram Bot client enables full battery telemetry monitoring, instant critical emergency alerts, and safe BMS switch controls from anywhere in the world **without requiring a public/static IP, port forwarding, dynamic DNS, or VPN/Tailscale configurations**.

```
┌──────────────┐         HTTPS (Port 443)         ┌─────────────────────┐
│   ESP32-C6   │ ───────────────────────────────> │ Telegram Bot API    │
│ (BMS Monitor)│ <─────────────────────────────── │ (api.telegram.org)  │
└──────────────┘         Long-Polling Updates     └──────────┬──────────┘
                                                             │
                                                             ▼
                                                  ┌─────────────────────┐
                                                  │  Your Smartphone/PC │
                                                  │  (Telegram Client)  │
                                                  └─────────────────────┘
```

### Step 1. Create Your Telegram Bot
1. Open Telegram and search for the official bot creator: [**@BotFather**](https://t.me/BotFather).
2. Click **Start** and send the command:
   ```text
   /newbot
   ```
3. Enter a display name for your bot (e.g., `My Home Battery Monitor`).
4. Choose a unique username ending in `bot` (e.g., `my_home_battery_bms_bot`).
5. `@BotFather` will reply with congratulations and your unique **HTTP API Token**:
   ```text
   1234567890:ABCdefGHIjklMNOpqrSTUvwxYZ_1234567
   ```
   *Keep this token safe — you will enter it into the monitor's web configuration.*

---

### Step 2. Retrieve Your Numeric Telegram User ID
Telegram safeguards user privacy and requires a unique **numeric Account ID** (not your public `@username`) for bots to address outbound messages.

1. Open [**@userinfobot**](https://t.me/userinfobot) or [**@myidbot**](https://t.me/myidbot) in Telegram.
2. Click **Start** (or send `/start` or `/getid`).
3. The bot will print your profile attributes. Copy the numeric value from the **Id** field:
   ```text
   Id: 123456789
   ```
> [!TIP]
> **Multi-Administrator Whitelist**:
> To authorize multiple family members or system administrators, provide a comma-separated list of User IDs:
> `123456789, 987654321`

---

### Step 3. ⚠️ CRITICAL REQUIREMENT: Initial Bot Launch (Starting the Chat)
> [!IMPORTANT]
> **Why is this step mandatory?**  
> Under Telegram's strict anti-spam privacy policy, a bot **cannot initiate a conversation or push messages to a user** until the user has opened a chat with the bot and pressed **START**.  
> If you enter your Bot Token and User ID into the monitor web interface without first initiating the chat in Telegram, Telegram will reject messages with:  
> `Bad Request: chat not found (User has not clicked START in the chat with this bot yet)`.

1. In Telegram, search for your newly created bot username (or open direct link `https://t.me/<your_bot_username>`).
2. Click the prominent **START** button at the bottom of the chat window (or send the text command `/start`).
3. Telegram now associates your User ID with the bot's conversation pool, enabling outbound notifications, interactive menus, and push alerts!

---

### Step 4. Configure the Monitor Web Interface
1. In your browser, open the monitor's setup dashboard: **`http://bms-monitor.local/setup`** (or `http://192.168.4.1/setup`).
2. Scroll to the **✈️ Telegram Bot Client** section.
3. Check **☑️ Enable Telegram Bot Client**.
4. Paste your token into **Telegram Bot Token (@BotFather)**.
5. Paste your numeric ID(s) into **Allowed Telegram User / Chat IDs**.
6. Check **☑️ Send Emergency Push Alerts (ΔV > 100mV, Overheat, Low SOC)**.
7. Click the **🔔 Test Notification** button:
   * The ESP32-C6 performs an immediate end-to-end verification handshake with Telegram servers.
   * Upon successful delivery, your Telegram app will instantly receive:
     ```text
     🔔 Test notification from Universal BMS Monitor ESP32-C6!
     Connection successful. Telegram Bot is online and ready.
     ```
   * A green confirmation message `✅ Test notification delivered successfully!` will appear in the web interface.
8. Click **💾 Save Configuration**. Once saved, the web header displays the **`✈️ TG Online`** status badge.

---

### Step 5. Telegram Bot Commands & Remote Management

The bot provides real-time diagnostic commands with inline keyboard buttons:

| Command | Description & Output |
|---|---|
| `/status` | **Full Battery Telemetry Report**: Total Pack Voltage (V), Battery Current with directional indicator (🟢 Charging / 🔴 Discharging / ⚪ Idle), Active Power (W), State of Charge (SOC%), Remaining Capacity (Ah), Cycle Count, Max Imbalance ($\Delta V$ in mV), MOSFET and Battery Temperatures, and live states of Charge MOS, Discharge MOS, and Balancer. |
| `/cells` | **Detailed Cell Voltage Matrix (1–32S)**: Individual cell voltages with 1 mV precision, automatic markers for minimum (`Vmin 🔻`), maximum (`Vmax 🔺`), active balancing markers (`⚡`), and total pack $\Delta V$. |
| `/control` | **MOSFET Power Switches & Balancer Remote**: Sends an interactive control keyboard for managing charging/discharging pathways and balancing. |
| `/info` | **Hardware & Connectivity Diagnostics**: ESP32-C6 Uptime, free RAM Heap, Wi-Fi RSSI signal strength, BLE link status, and firmware version. |
| `/help` | Detailed command manual and usage reference. |

#### 🛡️ Confirmation Guard for Power Switches
To eliminate accidental power interruptions or unexpected inverter shutdowns, every switch operation requires explicit confirmation:

1. Pressing, for example, `[⛔ Turn OFF Discharge]` displays:
   ```text
   ⚠️ ATTENTION! Are you sure you want to turn OFF Discharge MOS?
   This will immediately disconnect the battery load output!
   [✅ YES, TURN OFF]  [❌ CANCEL]
   ```
2. The BMS command executes **only** when the user taps `[✅ YES]`.
3. Tapping `[❌ CANCEL]` safely aborts the operation.

---

### Step 6. Proactive Emergency Push Alerts

When push alerts are enabled, the ESP32-C6 monitors telemetry 24/7 and dispatches instant priority notifications:

* 🚨 **High Cell Imbalance ($\Delta V > 100\text{ mV}$)**:
  * Triggers an immediate alert specifying the exact minimum and maximum cell voltages.
  * Sends an automatic resolution notice when balancing normalizes the delta ($\Delta V < 80\text{ mV}$).
* 🔥 **Overheat Warning ($T > 55^\circ\text{C}$)**:
  * Immediate notification if power MOSFET or battery probe temperatures exceed safe thresholds.
* 🪫 **Critical Low Battery ($\text{SOC} \le 10\%$ or $V_{\text{cell}} < 2.80\text{ V}$)**:
  * Alerts before deep discharge cut-off occurs, reporting remaining capacity and the weakest cell voltage.
* ⚠️ **BMS Bluetooth Link Interruption**:
  * Triggers if BLE communication ceases for longer than 45 seconds.

---

### Step 7. Non-Volatile Memory Guard (NVS Guard)

* **Isolated Flash Storage**:
  * Bot credentials (Token, User IDs, and active state) are stored within an isolated NVS namespace (`bms_tg`).
  * If you perform a factory reset of Wi-Fi / BMS settings by holding the hardware **BOOT** button (> 3 sec), **Telegram credentials remain securely preserved**.
* **Complete Removal**:
  * To permanently delete stored Telegram credentials from flash (e.g. before transferring the device), open `/setup` and click **[🗑️ Clear Telegram]**.

---

## 📄 License

This project is open-source and licensed under the permissive **MIT License**.
Developed for the off-grid and energy-independence community.
