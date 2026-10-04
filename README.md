# JK-JBD-BMS-Smart-Monitor-ESP32-C6

[![ESP32-C6](https://img.shields.io/badge/MCU-ESP32--C6%20%2F%20ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Protocol](https://img.shields.io/badge/Protocols-JK--BMS%20%7C%20JBD--BMS-brightgreen.svg)]()
[![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-orange.svg)](https://platformio.org/)
[![UI](https://img.shields.io/badge/WebUI-Dark%20SPA%20(UA%20%2F%20EN)-purple.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**Універсальний, повністю автономний розумний BLE-шлюз та локальний веб-монітор для акумуляторних батарей (LiFePO4 / Li-ion / LTO) на базі мікроконтролера ESP32-C6 (із підтримкою ESP32-S3).**

Підтримує одночасно два найпопулярніших протоколи BMS: **JK-BMS (JiKong)** та **JBD-BMS (Jiabaida / Xiaoxiang / Overkill Solar)** із вбудованою функцією автовизначення протоколу.

---

## ⚡ Головні особливості

* 🔄 **Універсальна підтримка протоколів (Dual BMS Protocol)**:
  * **JBD-BMS / Xiaoxiang / Overkill Solar**: робота через BLE Service `0xFF00`, підтримка запитів Basic Info (`0x03`), напруг комірок (`0x04`), імені пристрою (`0x05`), перевірка 16-бітного CRC та керування MOSFET-ключами (`0xE1`).
  * **JK-BMS / JiKong**: робота через BLE Service `0xFFE0`, декодування повного фрейму TLV (~300 байт), аутентифікація по PIN-коду, керування Charge MOS, Discharge MOS та активним балансиром.
  * **Auto-Detect**: автоматичне розпізнавання підключеного типу BMS за рекламними пакетами та сервісами Bluetooth.

* 🔋 **Повна автономність (100% Standalone)**:
  * Працює на одному мікроконтролері ESP32-C6 / ESP32-S3 без потреби у ПК, Raspberry Pi, Home Assistant чи інверторі.
  * Живлення від будь-якого джерела USB Type-C 5V або DC-DC конвертера.

* 🖥️ **Сучасний Dark SPA Web Dashboard**:
  * Адаптивний інтерфейс для смартфонів, планшетів та десктопів.
  * Двомовність: миттєве перемикання між **українською** та **англійською** мовами (🌐 UA / EN).
  * **Швидкі KPI**: Загальна напруга, струм батареї (з кольоровою індикацією заряд/розряд), активна потужність (W), рівень заряду (SOC%), залишкова та номінальна ємність (Ah), кількість циклів.
  * **Гістограма комірок (4S / 8S / 16S / 24S)**: напруга кожного осередку з роздільною здатністю 1 мВ, автоматичне підсвічування **MIN** та **MAX** осередків, динамічний розрахунок дельти напруг ($\Delta V$).
  * **Керування захистом**: інтерактивні перемикачі MOSFET Заряду (Charge MOS), MOSFET Розряду (Discharge MOS) та Активного Балансира.
  * **Статистика системи**: температури MOSFET та датчиків АКБ, рівень сигналу Wi-Fi RSSI, обсяг вільної оперативної пам'яті (Heap).

* 🌐 **Мережа та налаштування**:
  * Робота у домашній Wi-Fi мережі (Station Mode) + резервна точка доступу `BMS-Monitor-AP` (`192.168.4.1`) з Captive Portal.
  * Локальне mDNS ім'я: **`http://bms-monitor.local`**.
  * Вбудовані сканери Wi-Fi мереж та Bluetooth BMS пристроїв на сторінці налаштувань (`/setup`).

* 🔘 **Апаратне керування кнопкою BOOT (GPIO 9)**:
  * **Короткий клік (< 3 сек)**: примусовий реконнект Bluetooth та оновлення телеметрії.
  * **Довге затискання (> 3 сек)**: повне скидання налаштувань (Factory Reset) та повернення у режим точки доступу `BMS-Monitor-AP`.

---

## 📐 Архітектура проєкту

```
esp32c6_universal_bms_monitor/
├── bin/                               # Готові скомпільовані бінарні файли
│   ├── esp32c6_universal_bms_monitor_factory.bin   # Повний образ для ESP32-C6 (адреса 0x0000)
│   ├── esp32c6_universal_bms_monitor_app.bin       # Тільки прошивка для ESP32-C6 (адреса 0x10000)
│   ├── esp32s3_universal_bms_monitor_factory.bin   # Повний образ для ESP32-S3 (адреса 0x0000)
│   └── esp32s3_universal_bms_monitor_app.bin       # Тільки прошивка для ESP32-S3 (адреса 0x10000)
├── include/
│   ├── BmsBleClient.h                 # BLE Клієнт для роботи з JK та JBD
│   ├── BmsProtocol.h                  # Уніфікована структура телеметрії BmsTelemetry
│   ├── Config.h                       # NVS менеджер конфігурації
│   └── WebDashboard.h                 # HTML5/CSS3/JS Dark SPA Dashboard & Setup
├── src/
│   ├── BmsBleClient.cpp               # Реалізація BLE драйверів, парсинг, CRC
│   └── main.cpp                       # WebServer, Captive Portal, REST API, кнопка BOOT
├── platformio.ini                     # Збірка для esp32-c6-supermini та esp32-s3-supermini
└── README.md
```

---

## 📡 Опис REST API

| Метод | URL | Опис | Приклад корисного навантаження |
|---|---|---|---|
| `GET` | `/` | Головна панель моніторингу (SPA) | — |
| `GET` | `/setup` | Веб-сторінка налаштувань | — |
| `GET` | `/api/data` | Повна телеметрія батареї (JSON) | — |
| `POST` | `/api/switch` | Перемикання MOSFET ключів / балансира | `{"switch": "charging"|"discharging"|"balancer", "state": true|false}` |
| `POST` | `/api/set-cells` | Зміна кількості осередків | `{"cells": 8}` |
| `GET` | `/api/scan-wifi` | Сканування доступних Wi-Fi мереж | — |
| `GET` | `/api/scan-ble` | Пошук BMS в ефірі Bluetooth | — |
| `GET` | `/api/config` | Читання збережених налаштувань | — |
| `POST` | `/api/save-config` | Збереження нових налаштувань в NVS | `{"ssid":"...","pass":"...","mac":"...","type":0,"pin":"1234","cells":8}` |

---

## 🚀 Швидкий старт та Прошивка

### Варіант 1: Прошивка готового бінарника через `esptool`

Підключіть ESP32-C6 по USB та виконайте:

```bash
# 1. Повне очищення Flash (рекомендовано):
esptool.py --port /dev/ttyUSB0 erase_flash

# 2. Завантаження єдиного factory-бінарника (адреса 0x0000):
esptool.py --port /dev/ttyUSB0 --baud 921600 write_flash 0x0000 bin/esp32c6_universal_bms_monitor_factory.bin
```
*(Для macOS замініть `/dev/ttyUSB0` на `/dev/cu.usbmodem...`)*.

---

### Варіант 2: Збірка з вихідного коду (PlatformIO)

```bash
# Компіляція для ESP32-C6:
pio run -e esp32-c6-supermini

# Прошивка на підключену плату:
pio run -e esp32-c6-supermini -t upload

# Монітор Serial порту:
pio device monitor -b 115200
```

*(Для плат на базі ESP32-S3 використовуйте енвайронмент `-e esp32-s3-supermini`)*.

---

## ⚙️ Перше налаштування пристрою

1. Після першого увімкнення плата створить точку доступу Wi-Fi: **`BMS-Monitor-AP`** (без пароля).
2. Підключіться до неї з телефону або комп'ютера та відкрийте у браузері: **`http://192.168.4.1`**.
3. У формі налаштувань:
   * Виберіть вашу домашню мережу Wi-Fi та введіть пароль.
   * Натисніть **🔍 Find BMS** (або введіть MAC-адресу вручну).
   * Виберіть тип BMS (*Auto Detect*, *JK-BMS* або *JBD-BMS*) та кількість комірок (*4S, 8S, 16S, 24S*).
   * Натисніть **💾 Save & Connect**.
4. Пристрій перезавантажиться, підключиться до вашого Wi-Fi і стане доступним за адресою: **`http://bms-monitor.local`**.

---

## 📄 Ліцензія

Проєкт поширюється під відкритою ліцензією MIT.
Розроблено для спільноти енергонезалежності.
