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
        } else if (name.startsWith("JBD") || name.startsWith("Xiaoxiang") || name.startsWith("SP") ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("ff00")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FF00"))) {
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
    NimBLEDevice::init("BMS-Web-Monitor");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(new BmsScanCallbacks());
    pScan->setActiveScan(true);
    pScan->setInterval(80);
    pScan->setWindow(80); // 100% duty cycle for reliable scanner
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
    NimBLEDevice::getScan()->start(durationSeconds * 1000, false);
    Serial.printf("[BLE] Started background scan for %u seconds...\n", (unsigned int)durationSeconds);
}

String BmsBleClient::performScanSync(uint32_t durationSeconds) {
    m_discoveredDevices.clear();
    m_isScanning = true;
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(80);
    pScan->setWindow(80);
    Serial.printf("[BLE] Starting active scan for %u seconds...\n", durationSeconds);
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
            NimBLEAddress targetAddr(std::string(m_config.bms_mac.c_str()), BLE_ADDR_PUBLIC);
            if (!connectToDevice(targetAddr, m_config.bms_name, m_config.bms_type)) {
                String macStr = m_config.bms_mac;
                if (macStr.endsWith(":00") || macStr.endsWith(":00")) {
                    String altMac = macStr.substring(0, macStr.length() - 2) + "02";
                    NimBLEAddress altAddr(std::string(altMac.c_str()), BLE_ADDR_PUBLIC);
                    connectToDevice(altAddr, m_config.bms_name, m_config.bms_type);
                }
            }
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
            if (!m_isConnected && m_discoveredDevices.empty()) {
                startScan(4);
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
            sendJkPollRequest();
        }
    }
}

bool BmsBleClient::connectToDevice(const NimBLEAddress& address, const String& name, uint8_t forcedType) {
    Serial.printf("[BLE] Attempting connection to %s (type: %d, name: %s)...\n", 
                  address.toString().c_str(), address.getType(), name.c_str());

    if (m_pClient != nullptr) {
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
    }

    m_pClient = NimBLEDevice::createClient();
    m_pClient->setClientCallbacks(new BmsClientCallbacks());
    m_pClient->setConnectionParams(12, 12, 0, 200);
    m_pClient->setConnectTimeout(5000); // 5000 ms in NimBLE 2.x

    bool ok = m_pClient->connect(address);
    if (!ok) {
        // Try opposite address type (Random Static vs Public)
        uint8_t altType = (address.getType() == BLE_ADDR_PUBLIC) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
        NimBLEAddress altAddr(address.toString(), altType);
        Serial.printf("[BLE] Retrying with address type %d...\n", altType);
        ok = m_pClient->connect(altAddr);
    }

    if (!ok) {
        Serial.println("[BLE] Failed to connect.");
        return false;
    }

    Serial.println("[BLE] Connected! Discovering services...");
    m_rxBuffer.clear();

    // Try JBD Primary Service 0xFF00
    NimBLERemoteService* pJbdService = m_pClient->getService(NimBLEUUID("FF00"));
    if (pJbdService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JBD)) {
        Serial.println("[BLE] Found JBD Service (0xFF00)!");
        m_pJbdNotifyChar = pJbdService->getCharacteristic(NimBLEUUID("FF01"));
        m_pJbdWriteChar  = pJbdService->getCharacteristic(NimBLEUUID("FF02"));

        if (m_pJbdNotifyChar && m_pJbdWriteChar && m_pJbdNotifyChar->canNotify()) {
            m_pJbdNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleJbdPacket(pData, length);
            });

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "JBD-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "JBD-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // Request initial Device Name
            sendJbdCommand(0xA5, 0x05);
            return true;
        }
    }

    // Try JK Service 0xFFE0
    NimBLERemoteService* pJkService = m_pClient->getService(NimBLEUUID("FFE0"));
    if (pJkService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JK)) {
        Serial.println("[BLE] Found JK Service (0xFFE0)!");
        m_pJkChar = pJkService->getCharacteristic(NimBLEUUID("FFE1"));

        if (m_pJkChar && m_pJkChar->canNotify()) {
            m_pJkChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleJkPacket(pData, length);
            });

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "JK-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "JK-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();
            return true;
        }
    }

    Serial.println("[BLE] Neither JBD (0xFF00) nor JK (0xFFE0) characteristics were usable.");
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
    m_pJkChar        = nullptr;
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

    m_pJbdWriteChar->writeValue(frame, totalLen, false);
}

void BmsBleClient::handleJbdPacket(const uint8_t* data, size_t len) {
    if (len == 0) return;

    // Append to buffer for potential reassembly
    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    // Look for start 0xDD and end 0x77
    if (m_rxBuffer.size() < 7) return;

    // Find 0xDD
    while (!m_rxBuffer.empty() && m_rxBuffer[0] != 0xDD) {
        m_rxBuffer.erase(m_rxBuffer.begin());
    }

    if (m_rxBuffer.size() < 7) return;

    uint8_t reg = m_rxBuffer[1];
    uint8_t status = m_rxBuffer[2];
    uint8_t dataLen = m_rxBuffer[3];
    size_t expectedTotal = 7 + dataLen;

    if (m_rxBuffer.size() < expectedTotal) {
        return; // Waiting for more data
    }

    if (m_rxBuffer[expectedTotal - 1] != 0x77) {
        m_rxBuffer.erase(m_rxBuffer.begin());
        return;
    }

    m_telemetry.connected = true;
    m_telemetry.last_update = millis();

    // Parse according to register
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

            if (v < minV && v > 0.5f) { minV = v; minIdx = i; }
            if (v > maxV) { maxV = v; maxIdx = i; }
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

bool BmsBleClient::writeJbdFetState(uint8_t newFetMask) {
    if (!m_pJbdWriteChar || !m_isConnected) return false;
    uint8_t payload[2] = {0x00, (uint8_t)(newFetMask & 0x03)};
    sendJbdCommand(0x5A, 0xE1, payload, sizeof(payload));
    return true;
}

// ======================== JK Implementation ========================

void BmsBleClient::sendJkPollRequest() {
    if (!m_pJkChar || !m_isConnected) return;
    static const uint8_t req[20] = {
        0xAA, 0x55, 0x90, 0xEB, 0x96, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x11
    };
    m_pJkChar->writeValue(req, sizeof(req), false);
}

void BmsBleClient::handleJkPacket(const uint8_t* data, size_t len) {
    if (len == 0) return;
    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    // Look for JK header 0x55 0xAA 0xEB 0x90
    while (m_rxBuffer.size() >= 4) {
        if (m_rxBuffer[0] == 0x55 && m_rxBuffer[1] == 0xAA && m_rxBuffer[2] == 0xEB && m_rxBuffer[3] == 0x90) {
            break;
        }
        m_rxBuffer.erase(m_rxBuffer.begin());
    }

    if (m_rxBuffer.size() < 300) return; // JK frames are ~300 bytes

    m_telemetry.connected = true;
    m_telemetry.last_update = millis();

    // Parse JK TLV structures (0x79 for cell voltages, 0x83 for total voltage, 0x84 for current, 0x85 for SOC, etc.)
    size_t idx = 4;
    while (idx < m_rxBuffer.size() - 4) {
        uint8_t tag = m_rxBuffer[idx++];
        if (tag == 0x79) {
            // Cell voltages length
            uint8_t length = m_rxBuffer[idx++];
            uint8_t cellCount = length / 3;
            if (cellCount > 32) cellCount = 32;
            m_telemetry.cell_count = cellCount;

            float minV = 99.0f, maxV = 0.0f;
            uint8_t minIdx = 0, maxIdx = 0;

            for (int c = 0; c < cellCount; ++c) {
                uint8_t cIdx = m_rxBuffer[idx++];
                uint16_t cMv = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1];
                idx += 2;

                float v = cMv * 0.001f;
                if (c < 32) m_telemetry.cell_voltages[c] = v;

                if (v < minV && v > 0.5f) { minV = v; minIdx = c; }
                if (v > maxV) { maxV = v; maxIdx = c; }
            }
            m_telemetry.min_cell_v = (minV == 99.0f) ? 0.0f : minV;
            m_telemetry.max_cell_v = maxV;
            m_telemetry.delta_cell_v = (maxV >= minV && minV > 0.0f) ? (maxV - minV) : 0.0f;
            m_telemetry.min_cell_idx = minIdx;
            m_telemetry.max_cell_idx = maxIdx;

        } else if (tag == 0x80) { // MOS Temp
            uint16_t t = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
            m_telemetry.temp_mos = (t > 100) ? (t - 100) : (float)t;
        } else if (tag == 0x81) { // Temp 1
            uint16_t t = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
            m_telemetry.temp_sensor1 = (t > 100) ? (t - 100) : (float)t;
        } else if (tag == 0x82) { // Temp 2
            uint16_t t = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
            m_telemetry.temp_sensor2 = (t > 100) ? (t - 100) : (float)t;
        } else if (tag == 0x83) { // Total Voltage (10mV)
            uint16_t v = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
            m_telemetry.total_voltage = v * 0.01f;
        } else if (tag == 0x84) { // Current (10mA, sign bit in MSB)
            uint16_t curr = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
            float amp = (curr & 0x7FFF) * 0.01f;
            m_telemetry.current = (curr & 0x8000) ? -amp : amp;
            m_telemetry.power = m_telemetry.total_voltage * fabs(m_telemetry.current);
        } else if (tag == 0x85) { // SOC (%)
            m_telemetry.soc = m_rxBuffer[idx++];
        } else if (tag == 0x89) { // Cycles
            m_telemetry.cycle_count = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
        } else if (tag == 0x8B) { // Warnings
            m_telemetry.raw_errors = (m_rxBuffer[idx] << 8) | m_rxBuffer[idx + 1]; idx += 2;
        } else if (tag == 0x8D) { // Switches status (Bit0: Charge, Bit1: Discharge, Bit2: Balancer)
            uint8_t sw = m_rxBuffer[idx++];
            m_telemetry.switch_charging    = (sw & 0x01) != 0;
            m_telemetry.switch_discharging = (sw & 0x02) != 0;
            m_telemetry.switch_balancer    = (sw & 0x04) != 0;
        } else {
            // Unhandled single byte / tag, skip
            idx++;
        }
    }

    m_rxBuffer.clear();
}

bool BmsBleClient::writeJkRegister(uint8_t reg, uint8_t value) {
    if (!m_pJkChar || !m_isConnected) return false;
    uint8_t frame[20] = {0};
    frame[0] = 0xAA; frame[1] = 0x55; frame[2] = 0x90; frame[3] = 0xEB; frame[4] = 0x96;
    frame[5] = 0x01; // Write holding register
    frame[6] = reg;
    frame[7] = value;
    // Checksum = sum of bytes 0..18
    uint8_t crc = 0;
    for (int i = 0; i < 19; ++i) crc += frame[i];
    frame[19] = crc;

    m_pJkChar->writeValue(frame, sizeof(frame), false);
    return true;
}

// ======================== Universal Controls ========================

bool BmsBleClient::setCharging(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS") {
        uint8_t currentFet = (m_telemetry.switch_charging ? 0x01 : 0x00) | 
                             (m_telemetry.switch_discharging ? 0x02 : 0x00);
        if (enable) currentFet |= 0x01;
        else currentFet &= ~0x01;
        return writeJbdFetState(currentFet);
    } else if (m_telemetry.bms_type == "JK-BMS") {
        return writeJkRegister(29, enable ? 0x01 : 0x00);
    }
    return false;
}

bool BmsBleClient::setDischarging(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS") {
        uint8_t currentFet = (m_telemetry.switch_charging ? 0x01 : 0x00) | 
                             (m_telemetry.switch_discharging ? 0x02 : 0x00);
        if (enable) currentFet |= 0x02;
        else currentFet &= ~0x02;
        return writeJbdFetState(currentFet);
    } else if (m_telemetry.bms_type == "JK-BMS") {
        return writeJkRegister(30, enable ? 0x01 : 0x00);
    }
    return false;
}

bool BmsBleClient::setBalancer(bool enable) {
    if (m_telemetry.bms_type == "JK-BMS") {
        return writeJkRegister(31, enable ? 0x01 : 0x00);
    }
    return false; // JBD handles balancing autonomously
}
