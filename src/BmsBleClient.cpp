#include "BmsBleClient.h"
#include <ArduinoJson.h>

static const char* TAG = "BmsBleClient";

// Global pointer for static callbacks
static BmsBleClient* s_pInstance = nullptr;

class BmsClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("[BLE] Connected to BMS device!");
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        Serial.printf("[BLE] Disconnected from BMS (reason: %d)\n", reason);
        if (s_pInstance) {
            s_pInstance->disconnect();
        }
    }

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
        uint32_t pin = 123456;
        if (s_pInstance && s_pInstance->getConfig().bms_pin.length() > 0) {
            pin = s_pInstance->getConfig().bms_pin.toInt();
        }
        Serial.printf("[SEC] BLE Passkey requested by peripheral -> injecting PIN: %u\n", (unsigned int)pin);
        NimBLEDevice::injectPassKey(connInfo, pin);
    }

    uint32_t onPassKeyDisplay(NimBLEConnInfo& connInfo) override {
        uint32_t pin = 123456;
        if (s_pInstance && s_pInstance->getConfig().bms_pin.length() > 0) {
            pin = s_pInstance->getConfig().bms_pin.toInt();
        }
        Serial.printf("[SEC] BLE Passkey display -> returning PIN: %u\n", (unsigned int)pin);
        return pin;
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        if (connInfo.isEncrypted()) {
            Serial.println("[SEC] BLE Pairing / Authentication SUCCESSFUL (Encrypted link established)");
        } else {
            Serial.println("[SEC] BLE Pairing finished (Unencrypted link)");
        }
    }

    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pin) override {
        Serial.printf("[SEC] BLE Confirm Passkey PIN: %u\n", (unsigned int)pin);
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }
};

class BmsScanCallbacks : public NimBLEScanCallbacks {
    void processDevice(const NimBLEAdvertisedDevice* advertisedDevice) {
        if (!s_pInstance) return;

        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();
        int rssi = advertisedDevice->getRSSI();
        String type = "BLE Device";

        if (name.startsWith("JK") || advertisedDevice->isAdvertisingService(NimBLEUUID("ffe0")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FFE0"))) {
            type = "JK-BMS";
        } else if (name.startsWith("JBD") || name.startsWith("Xiaoxiang") || name.startsWith("SP") || name.startsWith("BS-") ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("ff00")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FF00")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("fff0")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FFF0")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("0000FF00-0000-1000-8000-00805F9B34FB")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("0000FFF0-0000-1000-8000-00805F9B34FB"))) {
            type = "JBD-BMS";
        }

        // Avoid duplicates
        for (auto& dev : s_pInstance->m_discoveredDevices) {
            if (dev.address.equalsIgnoreCase(addr)) {
                dev.rssi = rssi;
                if (dev.name.length() == 0 && name.length() > 0) dev.name = name;
                if (type != "BLE Device") dev.bms_type = type;
                return;
            }
        }

        BleDiscoveredDevice d;
        d.name = name.length() > 0 ? name : "BMS Device";
        d.address = addr;
        d.rssi = rssi;
        d.bms_type = type;
        d.addr_type = advertisedDevice->getAddress().getType();
        s_pInstance->m_discoveredDevices.push_back(d);

        Serial.printf("[SCAN] Discovered: %s [%s] RSSI:%d Type:%s\n",
                      d.name.c_str(), addr.c_str(), rssi, type.c_str());
    }

    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        processDevice(advertisedDevice);
    }

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        processDevice(advertisedDevice);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.printf("[SCAN] Scan finished (found %d devices, reason: %d).\n", results.getCount(), reason);
        if (s_pInstance) s_pInstance->m_isScanning = false;
    }
};

BmsBleClient::BmsBleClient() {
    s_pInstance = this;
}

bool BmsBleClient::init() {
    if (!m_mutex) {
        m_mutex = xSemaphoreCreateMutex();
    }
    NimBLEDevice::init("BMS-Web-Monitor");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, false);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new BmsScanCallbacks());
    pScan->setActiveScan(true);
    pScan->setInterval(160); // 100ms
    pScan->setWindow(40);    // 25ms (25% duty cycle for Wi-Fi AP coexistence)
    return true;
}

void BmsBleClient::setTargetConfig(const AppConfig& cfg) {
    m_config = cfg;
}

void BmsBleClient::startScan(uint32_t durationSeconds) {
    if (m_isScanning) return;
    m_discoveredDevices.clear();
    m_isScanning = true;
    m_lastScanStartTime = millis();
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setInterval(160);
    pScan->setWindow(40);
    pScan->start(durationSeconds * 1000, false);
    Serial.printf("[BLE] Started background scan for %u seconds...\n", (unsigned int)durationSeconds);
}

String BmsBleClient::performScanSync(uint32_t durationSeconds) {
    m_discoveredDevices.clear();
    m_isScanning = true;
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->clearResults();
    pScan->setActiveScan(true);
    pScan->setInterval(160);
    pScan->setWindow(50); // ~30% duty cycle so Wi-Fi stays responsive
    Serial.printf("[BLE] Starting active scan for %u seconds...\n", (unsigned int)durationSeconds);
    NimBLEScanResults results = pScan->getResults(durationSeconds * 1000, false);
    Serial.printf("[BLE] Scan complete. Found %d devices.\n", results.getCount());
    m_isScanning = false;
    return getDiscoveredDevicesJson();
}

std::vector<BleDiscoveredDevice> BmsBleClient::getDiscoveredDevices() {
    return m_discoveredDevices;
}

String BmsBleClient::getDiscoveredDevicesJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& dev : m_discoveredDevices) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = dev.name;
        obj["mac"] = dev.address;
        obj["rssi"] = dev.rssi;
        obj["type"] = dev.bms_type;
    }
    String json;
    serializeJson(doc, json);
    return json;
}

void BmsBleClient::loop() {
    uint32_t now = millis();

    // Check scan timeout
    if (m_isScanning && (now - m_lastScanStartTime > 7000)) {
        m_isScanning = false;
    }

    // Auto-connect if not connected and not scanning
    if (!m_isConnected && !m_isScanning && (now - m_lastConnectAttempt > 4000)) {
        m_lastConnectAttempt = now;

        if (m_config.bms_mac.length() > 0) {
            unsigned int firstByte = 0;
            sscanf(m_config.bms_mac.c_str(), "%02x", &firstByte);
            static bool s_tryAlt = false;
            uint8_t primaryType = ((firstByte & 0xC0) == 0xC0) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
            uint8_t secondaryType = (primaryType == BLE_ADDR_PUBLIC) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
            uint8_t useType = s_tryAlt ? secondaryType : primaryType;
            s_tryAlt = !s_tryAlt;

            NimBLEAddress targetAddr(std::string(m_config.bms_mac.c_str()), useType);
            connectToDevice(targetAddr, m_config.bms_name, m_config.bms_type);
        } else {
            // Auto search for known devices
            for (const auto& dev : m_discoveredDevices) {
                if (dev.bms_type != "Unknown") {
                    NimBLEAddress targetAddr(std::string(dev.address.c_str()), dev.addr_type);
                    if (connectToDevice(targetAddr, dev.name, m_config.bms_type)) {
                        break;
                    }
                }
            }
            // In unconfigured state, scan periodically (every 20s) instead of aggressive loop
            if (!m_isConnected && m_discoveredDevices.empty() && (now - m_lastScanStartTime > 20000)) {
                startScan(3);
            }
        }
    }

    // Poll connected BMS
    if (m_isConnected && (now - m_lastPollTime >= 1000)) {
        m_lastPollTime = now;

        if (m_telemetry.bms_type == "JBD-BMS") {
            // Alternate between Basic Info (0x03) and Cell Voltages (0x04)
            if (m_pollStep % 2 == 0) {
                sendJbdCommand(0xA5, 0x03); // Request Basic Info
            } else {
                sendJbdCommand(0xA5, 0x04); // Request Cell Voltages
            }
            m_pollStep++;
        } else if (m_telemetry.bms_type == "JK-BMS") {
            // If connected to JK-BMS, only poll if silent for > 4s
            if (m_telemetry.last_update == 0 || (now - m_telemetry.last_update > 4000)) {
                sendJkPollRequest();
            }
        }
    }
}

bool BmsBleClient::connectToDevice(const NimBLEAddress& address, const String& name, uint8_t forcedType) {
    Serial.printf("[BLE] Attempting connection to %s (type: %d, name: %s)...\n", 
                  address.toString().c_str(), address.getType(), name.c_str());

    if (m_pClient == nullptr) {
        m_pClient = NimBLEDevice::createClient();
        m_pClient->setClientCallbacks(new BmsClientCallbacks(), false);
        m_pClient->setConnectionParams(24, 48, 0, 400); // 30ms - 60ms interval, 4s timeout (Optimal for Wi-Fi coexistence)
        m_pClient->setConnectTimeout(1500);
    }

    bool ok = m_pClient->connect(address, false);
    if (!ok) {
        Serial.println("[BLE] Failed to connect with given address type.");
        return false;
    }

    Serial.println("[BLE] Connected! Discovering services...");
    m_rxBuffer.clear();

    // Discover all available remote services
    std::vector<NimBLERemoteService*> services = m_pClient->getServices(true);
    NimBLERemoteService* pJbdService = nullptr;
    NimBLERemoteService* pJkService = nullptr;

    for (auto* s : services) {
        String sUuid = s->getUUID().toString().c_str();
        sUuid.toLowerCase();
        Serial.printf("[BLE] Found Remote Service UUID: %s\n", sUuid.c_str());

        if (sUuid.indexOf("ff00") >= 0 || sUuid.indexOf("fff0") >= 0) {
            pJbdService = s;
        } else if (sUuid.indexOf("ffe0") >= 0) {
            pJkService = s;
        }
    }

    // 1. Try JBD Service (FF00 / FFF0 / 128-bit)
    if (pJbdService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JBD)) {
        Serial.printf("[BLE] Connecting to JBD Service (%s)...\n", pJbdService->getUUID().toString().c_str());
        if (m_config.bms_pin.length() > 0) {
            Serial.printf("[BLE] Securing JBD connection with Passkey: %s\n", m_config.bms_pin.c_str());
            m_pClient->secureConnection();
        }
        m_pJbdNotifyChar = nullptr;
        m_pJbdWriteChar  = nullptr;

        for (auto* c : pJbdService->getCharacteristics(true)) {
            String uuid = c->getUUID().toString().c_str();
            uuid.toLowerCase();
            Serial.printf("[BLE]   JBD Char UUID: %s, handle: %d, canNotify: %d, canIndicate: %d, canWrite: %d, canWriteNoResp: %d\n",
                          uuid.c_str(), c->getHandle(), c->canNotify(), c->canIndicate(), c->canWrite(), c->canWriteNoResponse());

            if (uuid.indexOf("ff01") >= 0 || uuid.indexOf("fff1") >= 0 || c->canNotify() || c->canIndicate()) {
                m_pJbdNotifyChar = c;
            }
            if (uuid.indexOf("ff02") >= 0 || uuid.indexOf("fff2") >= 0 || c->canWrite() || c->canWriteNoResponse()) {
                m_pJbdWriteChar = c;
            }
        }

        if (m_pJbdNotifyChar && m_pJbdWriteChar) {
            m_pJbdNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleJbdPacket(pData, length);
            });

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "JBD-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "JBD-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // 1. Send Handshake & Authentication Frame (Configured PIN + standard fallbacks)
            if (m_config.bms_pin.length() >= 4) {
                uint8_t pinLen = m_config.bms_pin.length();
                if (pinLen > 16) pinLen = 16;
                uint8_t customPinFrame[32];
                customPinFrame[0] = 0xFF;
                customPinFrame[1] = 0xAA;
                customPinFrame[2] = 0x15;
                customPinFrame[3] = pinLen;
                uint8_t sum = 0x15 + pinLen;
                for (size_t i = 0; i < pinLen; ++i) {
                    customPinFrame[4 + i] = m_config.bms_pin[i];
                    sum += m_config.bms_pin[i];
                }
                customPinFrame[4 + pinLen] = sum;
                Serial.printf("[BLE] Sending Configured JBD PIN Auth ('%s')...\n", m_config.bms_pin.c_str());
                m_pJbdWriteChar->writeValue(customPinFrame, 4 + pinLen + 1, !m_pJbdWriteChar->canWriteNoResponse());
                delay(80);
            }

            uint8_t authFrame123456[] = { 0xFF, 0xAA, 0x15, 0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x53 };
            m_pJbdWriteChar->writeValue(authFrame123456, sizeof(authFrame123456), !m_pJbdWriteChar->canWriteNoResponse());
            delay(80);

            uint8_t authFrame000000[] = { 0xFF, 0xAA, 0x15, 0x06, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3B };
            m_pJbdWriteChar->writeValue(authFrame000000, sizeof(authFrame000000), !m_pJbdWriteChar->canWriteNoResponse());
            delay(80);

            // 2. Send Status query (0x19)
            uint8_t statusFrame[] = { 0xFF, 0xAA, 0x19, 0x01, 0x01, 0x1B };
            m_pJbdWriteChar->writeValue(statusFrame, sizeof(statusFrame), !m_pJbdWriteChar->canWriteNoResponse());

            delay(150);

            // 3. Request initial Device Name
            sendJbdCommand(0xA5, 0x05);
            return true;
        }
    }

    // 2. Try JK Service (FFE0)
    if (pJkService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JK)) {
        Serial.println("[BLE] Found JK Service (0xFFE0)! Discovering characteristics...");
        m_pJkNotifyChar = nullptr;
        m_pJkWriteChar = nullptr;

        for (auto* c : pJkService->getCharacteristics(true)) {
            String uuid = c->getUUID().toString().c_str();
            uuid.toLowerCase();
            Serial.printf("[BLE]   JK Char UUID: %s, handle: %d, canNotify: %d, canWrite: %d, canWriteNoResp: %d\n",
                          uuid.c_str(), c->getHandle(), c->canNotify(), c->canWrite(), c->canWriteNoResponse());

            if (uuid.indexOf("ffe2") >= 0 || (c->canWriteNoResponse() && !c->canNotify())) {
                m_pJkWriteChar = c;
            }
            if (uuid.indexOf("ffe1") >= 0 || c->canNotify()) {
                m_pJkNotifyChar = c;
                c->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                    this->handleJkPacket(pData, length);
                });
                Serial.printf("[BLE]   Subscribed to JK notifications on handle %d\n", c->getHandle());
            }
        }

        if (!m_pJkWriteChar && m_pJkNotifyChar) {
            m_pJkWriteChar = m_pJkNotifyChar;
        }

        if (m_pJkNotifyChar) {
            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "JK-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "JK-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // 1. Handshake session with DeviceInfo (0x97)
            Serial.println("[BLE] Sending JK Handshake Request (0x97)...");
            auto frameDev = buildJkFrame(0x97, 0, 0);
            m_pJkNotifyChar->writeValue(frameDev.data(), frameDev.size(), false);

            delay(250);

            // 2. Request initial CellInfo (0x96)
            sendJkPollRequest();
            return true;
        }
    }

    Serial.println("[BLE] Neither JBD (0xFF00/0xFFF0) nor JK (0xFFE0) characteristics were usable.");
    disconnect();
    return false;
}

void BmsBleClient::disconnect() {
    if (m_pClient && m_pClient->isConnected()) {
        m_pClient->disconnect();
    }
    m_isConnected = false;
    m_telemetry.connected = false;
    m_pJbdNotifyChar = nullptr;
    m_pJbdWriteChar  = nullptr;
    m_pJkNotifyChar  = nullptr;
    m_pJkWriteChar   = nullptr;
    m_rxBuffer.clear();
}

void BmsBleClient::reconnect() {
    Serial.println("[BLE] Manual reconnection triggered.");
    disconnect();
    m_lastConnectAttempt = 0;
}

// ======================== JBD Implementation ========================

void BmsBleClient::sendJbdCommand(uint8_t cmd, uint8_t reg, const uint8_t* payload, uint8_t len) {
    if (!m_pJbdWriteChar || !m_isConnected) return;

    size_t totalLen = 7 + len;
    uint8_t frame[64];
    frame[0] = 0xDD;
    frame[1] = cmd;
    frame[2] = reg;
    frame[3] = len;

    if (payload && len > 0) {
        memcpy(&frame[4], payload, len);
    }

    // Request CRC: 0x10000 - sum(reg + len + payload)
    uint32_t sum = reg + len;
    for (size_t i = 0; i < len; ++i) sum += payload[i];
    uint16_t crc = (uint16_t)(0x10000 - sum);

    frame[4 + len] = (uint8_t)(crc >> 8);
    frame[5 + len] = (uint8_t)(crc & 0xFF);
    frame[6 + len] = 0x77;

    bool responseRequired = !m_pJbdWriteChar->canWriteNoResponse();
    m_pJbdWriteChar->writeValue(frame, totalLen, responseRequired);
}

void BmsBleClient::handleJbdPacket(const uint8_t* data, size_t len) {
    if (len == 0) return;

    // Append to buffer for potential reassembly
    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    // Prevent buffer runaway
    if (m_rxBuffer.size() > 512) {
        m_rxBuffer.clear();
        return;
    }

    // Process all complete packets in buffer
    while (!m_rxBuffer.empty()) {
        // 1. Check for Module Response (FF AA <cmd> <len> <data...> <chk>)
        if (m_rxBuffer.size() >= 2 && m_rxBuffer[0] == 0xFF && m_rxBuffer[1] == 0xAA) {
            if (m_rxBuffer.size() < 4) return; // Wait for cmd + len
            uint8_t modCmd = m_rxBuffer[2];
            uint8_t modLen = m_rxBuffer[3];
            size_t totalModLen = 4 + modLen + 1;
            if (m_rxBuffer.size() < totalModLen) return; // Wait for full payload + chk

            Serial.printf("[BLE] JBD Module Response 0x%02X (len %u) received\n", modCmd, modLen);
            m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + totalModLen);
            continue;
        }

        // 2. Find start of JBD Frame (0xDD)
        if (m_rxBuffer[0] != 0xDD) {
            m_rxBuffer.erase(m_rxBuffer.begin());
            continue;
        }

        // 3. Check for JBD Telemetry/Register Frame (DD <reg> <status> <len> ... 77)
        if (m_rxBuffer.size() < 7) return; // Wait for minimum frame

        uint8_t reg = m_rxBuffer[1];
        uint8_t status = m_rxBuffer[2];
        uint8_t dataLen = m_rxBuffer[3];
        size_t expectedTotal = 7 + dataLen;

        if (m_rxBuffer.size() < expectedTotal) {
            return; // Waiting for full register data
        }

        if (m_rxBuffer[expectedTotal - 1] != 0x77) {
            m_rxBuffer.erase(m_rxBuffer.begin());
            continue;
        }

        m_telemetry.connected = true;
        m_telemetry.last_update = millis();

        if (reg == 0x03 && dataLen >= 23) {
            // Basic Info
            const uint8_t* p = &m_rxBuffer[4];
            uint16_t rawV = (p[0] << 8) | p[1];
            int16_t  rawI = (int16_t)((p[2] << 8) | p[3]);
            uint16_t remCap = (p[4] << 8) | p[5];
            uint16_t nomCap = (p[6] << 8) | p[7];
            uint16_t cycles = (p[8] << 8) | p[9];
            uint8_t  soc = p[19];
            uint8_t  fet = p[20];
            uint8_t  cells = p[21];
            uint8_t  ntcCnt = p[22];

            m_telemetry.total_voltage = rawV * 0.01f;
            m_telemetry.current = rawI * 0.01f;
            m_telemetry.power = m_telemetry.total_voltage * fabs(m_telemetry.current);
            m_telemetry.capacity_remain = remCap * 0.01f;
            m_telemetry.capacity_nominal = nomCap * 0.01f;
            m_telemetry.cycle_count = cycles;
            m_telemetry.soc = soc;
            m_telemetry.cell_count = cells;

            m_telemetry.switch_charging = (fet & 0x01) != 0;
            m_telemetry.switch_discharging = (fet & 0x02) != 0;

            if (ntcCnt >= 1 && dataLen >= 25) {
                uint16_t t1 = (p[23] << 8) | p[24];
                m_telemetry.temp_sensor1 = (t1 - 2731) * 0.1f;
            }
            if (ntcCnt >= 2 && dataLen >= 27) {
                uint16_t t2 = (p[25] << 8) | p[26];
                m_telemetry.temp_sensor2 = (t2 - 2731) * 0.1f;
            }
        } else if (reg == 0x04) {
            // Cell Voltages
            const uint8_t* p = &m_rxBuffer[4];
            uint8_t numCells = dataLen / 2;
            if (numCells > 32) numCells = 32;
            m_telemetry.cell_count = numCells;

            float minV = 99.0f, maxV = 0.0f;
            uint8_t minIdx = 0, maxIdx = 0;

            for (int i = 0; i < numCells; ++i) {
                uint16_t mv = (p[i * 2] << 8) | p[i * 2 + 1];
                float v = mv * 0.001f;
                m_telemetry.cell_voltages[i] = v;

                if (v < minV && v > 0.5f) { minV = v; minIdx = i + 1; }
                if (v > maxV) { maxV = v; maxIdx = i + 1; }
            }

            m_telemetry.min_cell_v = (minV == 99.0f) ? 0.0f : minV;
            m_telemetry.max_cell_v = maxV;
            m_telemetry.delta_cell_v = (maxV >= minV && minV > 0.0f) ? (maxV - minV) : 0.0f;
            m_telemetry.min_cell_idx = minIdx;
            m_telemetry.max_cell_idx = maxIdx;
        } else if (reg == 0x05) {
            // Device Name
            String name = "";
            for (int i = 0; i < dataLen; ++i) {
                name += (char)m_rxBuffer[4 + i];
            }
            if (name.length() > 0) m_telemetry.device_name = name;
        }

        m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + expectedTotal);
    }
}

bool BmsBleClient::writeJbdFetState(uint8_t newFetMask) {
    if (!m_pJbdWriteChar || !m_isConnected) return false;
    uint8_t payload[2] = {0x00, (uint8_t)(newFetMask & 0x03)};
    sendJbdCommand(0x5A, 0xE1, payload, sizeof(payload));
    return true;
}

// ======================== JK Implementation ========================

uint8_t BmsBleClient::calcJkCrc(const uint8_t* data, size_t len) {
    uint8_t c = 0;
    for (size_t i = 0; i < len; i++) {
        c += data[i];
    }
    return c;
}

std::vector<uint8_t> BmsBleClient::buildJkFrame(uint8_t address, uint32_t value, uint8_t length) {
    std::vector<uint8_t> frame(20, 0x00);
    frame[0] = 0xAA;
    frame[1] = 0x55;
    frame[2] = 0x90;
    frame[3] = 0xEB;
    frame[4] = address;
    frame[5] = length;
    frame[6] = (value >> 0) & 0xFF;
    frame[7] = (value >> 8) & 0xFF;
    frame[8] = (value >> 16) & 0xFF;
    frame[9] = (value >> 24) & 0xFF;
    frame[19] = calcJkCrc(frame.data(), 19);
    return frame;
}

void BmsBleClient::sendJkPollRequest() {
    if (!m_isConnected) return;
    NimBLERemoteCharacteristic* target = m_pJkNotifyChar ? m_pJkNotifyChar : m_pJkWriteChar;
    if (!target) return;
    auto frame = buildJkFrame(0x96, 0, 0);
    target->writeValue(frame.data(), frame.size(), false);
}

void BmsBleClient::handleJkPacket(const uint8_t* data, size_t len) {
    if (len == 0) return;

    if (m_rxBuffer.size() > 500) {
        m_rxBuffer.clear();
    }
    // If packet starts with JK response header 0x55 0xAA 0xEB 0x90, reset buffer
    if (len >= 4 && data[0] == 0x55 && data[1] == 0xAA && data[2] == 0xEB && data[3] == 0x90) {
        m_rxBuffer.clear();
    }

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    if (m_rxBuffer.size() >= 5 && m_rxBuffer[0] == 0x55 && m_rxBuffer[1] == 0xAA && m_rxBuffer[2] == 0xEB && m_rxBuffer[3] == 0x90) {
        uint8_t frameType = m_rxBuffer[4];
        if (frameType == 0x01 && m_rxBuffer.size() >= 130) {
            decodeJkSettings(m_rxBuffer);
            m_rxBuffer.clear();
        } else if (frameType == 0x02 && m_rxBuffer.size() >= 300) {
            decodeJkCellInfo(m_rxBuffer);
            m_rxBuffer.clear();
        }
    }
}

void BmsBleClient::decodeJkCellInfo(const std::vector<uint8_t>& data) {
    if (data.size() < 300) return;

    auto get16 = [&](size_t i) -> uint16_t {
        if (i + 1 >= data.size()) return 0;
        return (uint16_t(data[i + 1]) << 8) | uint16_t(data[i]);
    };
    auto get32 = [&](size_t i) -> uint32_t {
        return (uint32_t(get16(i + 2)) << 16) | uint32_t(get16(i));
    };

    // Detect 32S offset vs 24S offset:
    // In JK02_32S total voltage is at 118 + 32 = 150.
    // In JK02_24S total voltage is at 118.
    size_t offset = 0;
    float v32 = (float)get32(150) * 0.001f;
    float v24 = (float)get32(118) * 0.001f;
    if (v32 >= 8.0f && v32 <= 80.0f) {
        offset = 32; // JK02_32S protocol
    } else if (v24 >= 8.0f && v24 <= 80.0f) {
        offset = 0;  // JK02_24S protocol
    } else {
        offset = 32; // Default to 32S
    }

    // Cell Voltages (bytes 6 + i * 2)
    float minV = 999.0f;
    float maxV = 0.0f;
    uint8_t minIdx = 1;
    uint8_t maxIdx = 1;
    uint8_t detectedCells = 0;

    for (int i = 0; i < 32; i++) {
        float v = (float)get16(6 + i * 2) * 0.001f;
        if (v >= 0.5f && v <= 5.0f) {
            m_telemetry.cell_voltages[i] = v;
            detectedCells = i + 1;
            if (v < minV) {
                minV = v;
                minIdx = i + 1;
            }
            if (v > maxV) {
                maxV = v;
                maxIdx = i + 1;
            }
        } else {
            m_telemetry.cell_voltages[i] = 0.0f;
        }
    }

    if (detectedCells > 0) {
        m_telemetry.cell_count = detectedCells;
    } else if (m_config.cell_count > 0) {
        m_telemetry.cell_count = m_config.cell_count;
    }

    m_telemetry.min_cell_v = (minV < 900.0f) ? minV : 0.0f;
    m_telemetry.max_cell_v = maxV;
    m_telemetry.delta_cell_v = (maxV > minV && minV < 900.0f) ? (maxV - minV) : 0.0f;
    m_telemetry.min_cell_idx = minIdx;
    m_telemetry.max_cell_idx = maxIdx;

    m_telemetry.total_voltage = (float)get32(118 + offset) * 0.001f;

    // Current (signed 32-bit: positive = charge, negative = discharge)
    int32_t rawCurrent = (int32_t)get32(126 + offset);
    m_telemetry.current = (float)rawCurrent * 0.001f;
    m_telemetry.power = m_telemetry.total_voltage * m_telemetry.current;

    // Temperatures
    m_telemetry.temp_sensor1 = (float)((int16_t)get16(130 + offset)) * 0.1f;
    m_telemetry.temp_sensor2 = (float)((int16_t)get16(132 + offset)) * 0.1f;
    if (offset == 32) {
        m_telemetry.temp_mos = (float)((int16_t)get16(112 + offset)) * 0.1f;
    } else {
        m_telemetry.temp_mos = (float)((int16_t)get16(134 + offset)) * 0.1f;
    }

    // Balancer
    m_telemetry.balancing_current = (float)((int16_t)get16(138 + offset)) * 0.001f;
    uint8_t balState = (140 + offset < data.size()) ? data[140 + offset] : 0;
    m_telemetry.balancing_active = (balState != 0);

    // SOC & Capacity
    if (141 + offset < data.size()) {
        m_telemetry.soc = (float)data[141 + offset];
    }
    m_telemetry.capacity_remain = (float)get32(142 + offset) * 0.001f;
    m_telemetry.capacity_nominal = (float)get32(146 + offset) * 0.001f;

    // Cycle Count
    m_telemetry.cycle_count = get32(150 + offset);

    // Errors bitmask
    uint32_t errs = get32(134 + offset);
    m_telemetry.raw_errors = errs;
    m_telemetry.errors_str = (errs == 0) ? "OK" : ("0x" + String(errs, HEX));

    // Real-time switch states from live cell info frame
    if (167 + offset < data.size()) {
        m_telemetry.switch_charging    = (data[166 + offset] != 0);
        m_telemetry.switch_discharging = (data[167 + offset] != 0);
    }

    m_telemetry.connected = true;
    m_telemetry.last_update = millis();

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.printf("[BLE] JK Telemetry (%dS): V_tot=%.2fV, I=%.2fA, SOC=%.0f%%, Delta=%.3fV (Min=C%d:%.3fV, Max=C%d:%.3fV)\n",
                      m_telemetry.cell_count,
                      m_telemetry.total_voltage, m_telemetry.current, m_telemetry.soc,
                      m_telemetry.delta_cell_v,
                      m_telemetry.min_cell_idx, m_telemetry.min_cell_v,
                      m_telemetry.max_cell_idx, m_telemetry.max_cell_v);
    }
}

void BmsBleClient::decodeJkSettings(const std::vector<uint8_t>& data) {
    if (data.size() < 130) return;
    // Charge switch at 118, Discharge switch at 122, Balancer switch at 126
    m_telemetry.switch_charging    = (data[118] != 0);
    m_telemetry.switch_discharging = (data[122] != 0);
    m_telemetry.switch_balancer    = (data[126] != 0);
    m_telemetry.last_update = millis();
    Serial.printf("[BLE] JK Settings: Charge=%d, Discharge=%d, Balancer=%d\n",
                  m_telemetry.switch_charging, m_telemetry.switch_discharging, m_telemetry.switch_balancer);
}

bool BmsBleClient::writeJkRegister(uint8_t reg, uint32_t value, uint8_t length) {
    if (!m_isConnected) {
        Serial.println("[BLE] writeJkRegister failed: not connected");
        return false;
    }
    auto frame = buildJkFrame(reg, value, length);
    Serial.printf("[BLE] Setting JK register %u (0x%02X) to %u (len %u)\n", reg, reg, value, length);

    bool ok = false;
    NimBLERemoteCharacteristic* target = m_pJkNotifyChar;
    if (!target || (!target->canWrite() && !target->canWriteNoResponse())) {
        target = m_pJkWriteChar;
    }

    if (target) {
        if (target->canWriteNoResponse()) {
            ok = target->writeValue(frame.data(), frame.size(), false);
        } else if (target->canWrite()) {
            ok = target->writeValue(frame.data(), frame.size(), true);
        }
    }

    if (!ok && m_pJkWriteChar && m_pJkWriteChar != target) {
        ok = m_pJkWriteChar->writeValue(frame.data(), frame.size(), false);
    }

    if (ok) {
        if (reg == 29) m_telemetry.switch_charging = (value != 0);
        else if (reg == 30) m_telemetry.switch_discharging = (value != 0);
        else if (reg == 31) m_telemetry.switch_balancer = (value != 0);
    }
    return ok;
}

// ======================== Universal Controls ========================

bool BmsBleClient::setCharging(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS") {
        uint8_t currentFet = (m_telemetry.switch_charging ? 0x01 : 0x00) | 
                             (m_telemetry.switch_discharging ? 0x02 : 0x00);
        if (enable) currentFet |= 0x01;
        else currentFet &= ~0x01;
        bool ok = writeJbdFetState(currentFet);
        if (ok) m_telemetry.switch_charging = enable;
        return ok;
    } else {
        bool ok = writeJkRegister(29, enable ? 1 : 0, 4); // JK02 Reg 29: Charge Switch
        if (ok) m_telemetry.switch_charging = enable;
        return ok;
    }
}

bool BmsBleClient::setDischarging(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS") {
        uint8_t currentFet = (m_telemetry.switch_charging ? 0x01 : 0x00) | 
                             (m_telemetry.switch_discharging ? 0x02 : 0x00);
        if (enable) currentFet |= 0x02;
        else currentFet &= ~0x02;
        bool ok = writeJbdFetState(currentFet);
        if (ok) m_telemetry.switch_discharging = enable;
        return ok;
    } else {
        bool ok = writeJkRegister(30, enable ? 1 : 0, 4); // JK02 Reg 30: Discharge Switch
        if (ok) m_telemetry.switch_discharging = enable;
        return ok;
    }
}

bool BmsBleClient::setBalancer(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS") {
        return false; // JBD handles balancing autonomously
    } else {
        bool ok = writeJkRegister(31, enable ? 1 : 0, 4); // JK02 Reg 31: Active Balancer Switch
        if (ok) m_telemetry.switch_balancer = enable;
        return ok;
    }
}
