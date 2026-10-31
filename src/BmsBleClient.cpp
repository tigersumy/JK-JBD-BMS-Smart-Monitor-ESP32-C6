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

        if (name.startsWith("DL-") || name.startsWith("DL_") || name.startsWith("Daly") || name.startsWith("DALY") || name.startsWith("DL") || name.startsWith("BMS-") ||
            advertisedDevice->isAdvertisingService(NimBLEUUID("02f00000-0000-0000-0000-00000000fe00")) ||
            advertisedDevice->isAdvertisingService(NimBLEUUID("fe00")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FE00"))) {
            type = "Daly-BMS";
        } else if (name.startsWith("ANT") || name.startsWith("ant") || name.indexOf("ANT-BMS") >= 0 || name.indexOf("Ant") >= 0) {
            type = "Ant-BMS";
        } else if (name.startsWith("RN") || name.startsWith("LT") || name.startsWith("R-") || name.startsWith("L-") ||
                   name.indexOf("Redodo") >= 0 || name.indexOf("LiTime") >= 0 || name.indexOf("redodo") >= 0 || name.indexOf("litime") >= 0) {
            type = "Redodo-BMS";
        } else if (name.startsWith("JK") || advertisedDevice->isAdvertisingService(NimBLEUUID("ffe0")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FFE0"))) {
            if (name.indexOf("Daly") >= 0 || name.indexOf("DALY") >= 0 || name.startsWith("DL")) {
                type = "Daly-BMS";
            } else if (name.startsWith("ANT") || name.startsWith("ant") || name.indexOf("Ant") >= 0) {
                type = "Ant-BMS";
            } else if (name.startsWith("RN") || name.startsWith("LT") || name.indexOf("Redodo") >= 0 || name.indexOf("LiTime") >= 0) {
                type = "Redodo-BMS";
            } else {
                type = "JK-BMS";
            }
        } else if (name.startsWith("JBD") || name.startsWith("Xiaoxiang") || name.startsWith("SP") || name.startsWith("BS-") || name.startsWith("Smart") ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("ff00")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FF00")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("fee7")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FEE7")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("0000FF00-0000-1000-8000-00805F9B34FB"))) {
            type = "JBD-BMS";
        } else if (advertisedDevice->isAdvertisingService(NimBLEUUID("fff0")) || advertisedDevice->isAdvertisingService(NimBLEUUID("FFF0")) ||
                   advertisedDevice->isAdvertisingService(NimBLEUUID("0000FFF0-0000-1000-8000-00805F9B34FB"))) {
            if (name.startsWith("DL") || name.indexOf("Daly") >= 0 || name.indexOf("DALY") >= 0 || name.startsWith("BMS-")) {
                type = "Daly-BMS";
            } else if (name.startsWith("ANT") || name.startsWith("ant") || name.indexOf("Ant") >= 0) {
                type = "Ant-BMS";
            } else {
                type = "JBD-BMS";
            }
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
    if (m_isConnected) {
        if (m_telemetry.bms_type == "Daly-BMS") {
            if (now - m_lastPollTime >= 200) {
                m_lastPollTime = now;
                static const uint8_t s_dalyCmds[] = { 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98 };
                uint8_t cmd = s_dalyCmds[m_pollStep % (sizeof(s_dalyCmds)/sizeof(s_dalyCmds[0]))];
                sendDalyCommand(cmd);
                m_pollStep++;
            }
        } else if (now - m_lastPollTime >= 1000) {
            m_lastPollTime = now;

            if (m_telemetry.bms_type == "JBD-BMS") {
                // Alternate between Basic Info (0x03) and Cell Voltages (0x04)
                // Periodically request Device Name (0x05) if not yet fetched
                if (m_pollStep % 10 == 9 && (m_telemetry.device_name.length() == 0 || m_telemetry.device_name == "JBD-BMS")) {
                    sendJbdCommand(0xA5, 0x05); // Request Device Name
                } else if (m_pollStep % 2 == 0) {
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
            } else if (m_telemetry.bms_type == "Ant-BMS") {
                sendAntPollRequest();
            } else if (m_telemetry.bms_type == "Redodo-BMS") {
                sendRedodoPollRequest();
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

    bool hasDalyFe00 = false;

    for (auto* s : services) {
        String sUuid = s->getUUID().toString().c_str();
        sUuid.toLowerCase();
        Serial.printf("[BLE] Found Remote Service UUID: %s\n", sUuid.c_str());

        if (sUuid.indexOf("ff00") >= 0 || sUuid.indexOf("fff0") >= 0) {
            pJbdService = s;
        } else if (sUuid.indexOf("ffe0") >= 0) {
            pJkService = s;
        }
        if (sUuid.indexOf("fe00") >= 0 || sUuid.indexOf("02f00000") >= 0) {
            hasDalyFe00 = true;
        }
    }

    // 1. Try Daly BMS (Priority over JBD on shared FFF0 service)
    bool isDalyCandidate = (forcedType == BMS_TYPE_DALY) || 
                           hasDalyFe00 ||
                           ((forcedType == BMS_TYPE_AUTO) && (name.startsWith("DL") || name.startsWith("BMS-") || name.indexOf("Daly") >= 0 || name.indexOf("DALY") >= 0));

    bool isAntCandidate = (forcedType == BMS_TYPE_ANT) ||
                          ((forcedType == BMS_TYPE_AUTO) && (name.startsWith("ANT") || name.startsWith("ant") || name.indexOf("Ant") >= 0 || name.indexOf("ANT") >= 0));

    bool isRedodoCandidate = (forcedType == BMS_TYPE_REDODO) ||
                             ((forcedType == BMS_TYPE_AUTO) && (name.startsWith("RN") || name.startsWith("LT") || name.startsWith("R-") || name.startsWith("L-") || name.indexOf("Redodo") >= 0 || name.indexOf("LiTime") >= 0 || name.indexOf("redodo") >= 0 || name.indexOf("litime") >= 0));

    if (isDalyCandidate) {
        Serial.println("[BLE] Attempting Daly BMS protocol negotiation...");
        m_pDalyNotifyChar = nullptr;
        m_pDalyWriteChar = nullptr;

        for (auto* s : services) {
            for (auto* c : s->getCharacteristics(true)) {
                String uuid = c->getUUID().toString().c_str();
                uuid.toLowerCase();
                if (uuid.indexOf("fff1") >= 0 || uuid.indexOf("ffe1") >= 0 || uuid.indexOf("6e400003") >= 0) {
                    m_pDalyNotifyChar = c;
                }
                if (uuid.indexOf("fff2") >= 0 || uuid.indexOf("ffe2") >= 0 || uuid.indexOf("6e400002") >= 0) {
                    m_pDalyWriteChar = c;
                }
            }
        }

        if (!m_pDalyNotifyChar || !m_pDalyWriteChar) {
            for (auto* s : services) {
                String sUuid = s->getUUID().toString().c_str();
                sUuid.toLowerCase();
                if (sUuid.indexOf("fe00") >= 0 && services.size() > 1) continue;

                for (auto* c : s->getCharacteristics(true)) {
                    if (!m_pDalyNotifyChar && (c->canNotify() || c->canIndicate())) {
                        m_pDalyNotifyChar = c;
                    }
                    if (!m_pDalyWriteChar && (c->canWrite() || c->canWriteNoResponse())) {
                        m_pDalyWriteChar = c;
                    }
                }
            }
        }

        if (!m_pDalyWriteChar && m_pDalyNotifyChar && (m_pDalyNotifyChar->canWrite() || m_pDalyNotifyChar->canWriteNoResponse())) {
            m_pDalyWriteChar = m_pDalyNotifyChar;
        }

        if (m_pDalyNotifyChar && m_pDalyWriteChar) {
            m_pDalyNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleDalyPacket(pData, length);
            });
            m_dalyWriteWithResponse = m_pDalyWriteChar->canWrite() && !m_pDalyWriteChar->canWriteNoResponse();
            Serial.printf("[BLE] Selected Daly BMS: Notify H:%d, Write H:%d (WithResponse: %d)\n",
                          m_pDalyNotifyChar->getHandle(), m_pDalyWriteChar->getHandle(), m_dalyWriteWithResponse);

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "Daly-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "Daly-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // Initial query
            sendDalyCommand(0x90);
            delay(100);
            sendDalyCommand(0x94);
            return true;
        } else {
            Serial.println("[BLE] Daly candidate detected but characteristics not resolved, checking other protocols...");
        }
    }

    // 2. Try Ant BMS (service 0xFFE0, 0xFFF0, or NUS)
    if (isAntCandidate) {
        Serial.println("[BLE] Attempting Ant BMS protocol negotiation...");
        m_pAntNotifyChar = nullptr;
        m_pAntWriteChar = nullptr;

        for (auto* s : services) {
            for (auto* c : s->getCharacteristics(true)) {
                String uuid = c->getUUID().toString().c_str();
                uuid.toLowerCase();
                if (uuid.indexOf("ffe1") >= 0 || uuid.indexOf("fff1") >= 0 || uuid.indexOf("6e400003") >= 0) {
                    m_pAntNotifyChar = c;
                }
                if (uuid.indexOf("ffe2") >= 0 || uuid.indexOf("fff2") >= 0 || uuid.indexOf("6e400002") >= 0) {
                    m_pAntWriteChar = c;
                }
            }
        }

        if (!m_pAntNotifyChar || !m_pAntWriteChar) {
            for (auto* s : services) {
                for (auto* c : s->getCharacteristics(true)) {
                    if (!m_pAntNotifyChar && (c->canNotify() || c->canIndicate())) {
                        m_pAntNotifyChar = c;
                    }
                    if (!m_pAntWriteChar && (c->canWrite() || c->canWriteNoResponse())) {
                        m_pAntWriteChar = c;
                    }
                }
            }
        }

        if (!m_pAntWriteChar && m_pAntNotifyChar && (m_pAntNotifyChar->canWrite() || m_pAntNotifyChar->canWriteNoResponse())) {
            m_pAntWriteChar = m_pAntNotifyChar;
        }

        if (m_pAntNotifyChar && m_pAntWriteChar) {
            m_pAntNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleAntPacket(pData, length);
            });
            Serial.printf("[BLE] Selected Ant BMS: Notify H:%d, Write H:%d\n",
                          m_pAntNotifyChar->getHandle(), m_pAntWriteChar->getHandle());

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "Ant-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "Ant-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // Initial query
            sendAntPollRequest();
            return true;
        } else {
            Serial.println("[BLE] Ant candidate detected but characteristics not resolved, checking other protocols...");
        }
    }

    // 3. Try Redodo / LiTime BMS (service 0xFFE0, Notify 0xFFE1, Write 0xFFE2/0xFFE1)
    if (isRedodoCandidate) {
        Serial.println("[BLE] Attempting Redodo / LiTime BMS protocol negotiation...");
        m_pRedodoNotifyChar = nullptr;
        m_pRedodoWriteChar = nullptr;

        for (auto* s : services) {
            for (auto* c : s->getCharacteristics(true)) {
                String uuid = c->getUUID().toString().c_str();
                uuid.toLowerCase();
                if (uuid.indexOf("ffe1") >= 0) {
                    m_pRedodoNotifyChar = c;
                }
                if (uuid.indexOf("ffe2") >= 0) {
                    m_pRedodoWriteChar = c;
                }
            }
        }

        if (!m_pRedodoNotifyChar || !m_pRedodoWriteChar) {
            for (auto* s : services) {
                for (auto* c : s->getCharacteristics(true)) {
                    if (!m_pRedodoNotifyChar && (c->canNotify() || c->canIndicate())) {
                        m_pRedodoNotifyChar = c;
                    }
                    if (!m_pRedodoWriteChar && (c->canWrite() || c->canWriteNoResponse())) {
                        m_pRedodoWriteChar = c;
                    }
                }
            }
        }

        if (!m_pRedodoWriteChar && m_pRedodoNotifyChar && (m_pRedodoNotifyChar->canWrite() || m_pRedodoNotifyChar->canWriteNoResponse())) {
            m_pRedodoWriteChar = m_pRedodoNotifyChar;
        }

        if (m_pRedodoNotifyChar && m_pRedodoWriteChar) {
            m_pRedodoNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
                this->handleRedodoPacket(pData, length);
            });
            Serial.printf("[BLE] Selected Redodo BMS: Notify H:%d, Write H:%d\n",
                          m_pRedodoNotifyChar->getHandle(), m_pRedodoWriteChar->getHandle());

            m_isConnected = true;
            m_telemetry.connected = true;
            m_telemetry.bms_type = "Redodo-BMS";
            m_telemetry.device_name = name.length() > 0 ? name : "Redodo-BMS";
            m_telemetry.mac_address = address.toString().c_str();
            m_telemetry.last_update = millis();

            // Initial query
            sendRedodoPollRequest();
            return true;
        } else {
            Serial.println("[BLE] Redodo candidate detected but characteristics not resolved, checking other protocols...");
        }
    }

    // 4. Try JBD Service (FF00 / FFF0 / 128-bit)
    if (pJbdService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JBD) && (!isDalyCandidate || forcedType == BMS_TYPE_JBD) && !isAntCandidate) {
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

    // 5. Try JK Service (FFE0)
    if (pJkService && (forcedType == BMS_TYPE_AUTO || forcedType == BMS_TYPE_JK) && !isAntCandidate && !isRedodoCandidate) {
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
            NimBLERemoteCharacteristic* writeChar = m_pJkWriteChar ? m_pJkWriteChar : m_pJkNotifyChar;
            if (writeChar) {
                writeChar->writeValue(frameDev.data(), frameDev.size(), false);
            }

            delay(250);

            // 2. Request initial CellInfo (0x96)
            sendJkPollRequest();
            return true;
        }
    }

    Serial.println("[BLE] None of Daly, Ant, Redodo, JBD, or JK characteristics were usable.");
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
    m_pDalyNotifyChar = nullptr;
    m_pDalyWriteChar  = nullptr;
    m_pAntNotifyChar = nullptr;
    m_pAntWriteChar  = nullptr;
    m_pRedodoNotifyChar = nullptr;
    m_pRedodoWriteChar  = nullptr;
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

        // 4. Verify JBD CRC-16: 0x10000 - sum(bytes from status through payload)
        uint32_t rxSum = 0;
        for (size_t i = 2; i < expectedTotal - 3; ++i) {
            rxSum += m_rxBuffer[i];
        }
        uint16_t expectedCrc = (uint16_t)(0x10000 - rxSum);
        uint16_t packetCrc = ((uint16_t)m_rxBuffer[expectedTotal - 3] << 8) | m_rxBuffer[expectedTotal - 2];
        if (expectedCrc != packetCrc) {
            Serial.printf("[BLE] JBD CRC mismatch for Reg 0x%02X: got 0x%04X, expected 0x%04X\n", reg, packetCrc, expectedCrc);
            m_rxBuffer.erase(m_rxBuffer.begin());
            continue;
        }

        // 5. Check response status (0x00 = Success)
        if (status != 0x00) {
            Serial.printf("[BLE] JBD Reg 0x%02X returned error status: 0x%02X\n", reg, status);
            m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + expectedTotal);
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

            // Production Date: Year = (raw >> 9) + 2000, Month = (raw >> 5) & 0x0F, Day = raw & 0x1F
            if (dataLen >= 12) {
                uint16_t pDate = (p[10] << 8) | p[11];
                if (pDate != 0) {
                    uint16_t year = (pDate >> 9) + 2000;
                    uint8_t month = (pDate >> 5) & 0x0F;
                    uint8_t day = pDate & 0x1F;
                    char dBuf[16];
                    snprintf(dBuf, sizeof(dBuf), "%04u-%02u-%02u", year, month, day);
                    m_telemetry.prod_date = String(dBuf);
                }
            }

            // Temperature sensors (NTC 1, NTC 2, and NTC 3 - MOSFET temperature)
            if (ntcCnt >= 1 && dataLen >= 25) {
                uint16_t t1 = (p[23] << 8) | p[24];
                m_telemetry.temp_sensor1 = (t1 - 2731) * 0.1f;
                m_telemetry.temp_mos = m_telemetry.temp_sensor1;
            }
            if (ntcCnt >= 2 && dataLen >= 27) {
                uint16_t t2 = (p[25] << 8) | p[26];
                m_telemetry.temp_sensor2 = (t2 - 2731) * 0.1f;
                m_telemetry.temp_mos = m_telemetry.temp_sensor2;
            }
            if (ntcCnt >= 3 && dataLen >= 29) {
                uint16_t t3 = (p[27] << 8) | p[28];
                m_telemetry.temp_mos = (t3 - 2731) * 0.1f;
            }

            // 1. Balancing Bitmask (bytes 12..15: bit 0 = Cell 1, bit 1 = Cell 2, etc.)
            uint32_t balMask = 0;
            if (dataLen >= 14) {
                balMask = ((uint32_t)p[12] << 8) | p[13];
                if (dataLen >= 16 && (p[14] != 0 || p[15] != 0)) {
                    balMask |= (((uint32_t)p[14] << 24) | ((uint32_t)p[15] << 16));
                }
            }
            m_telemetry.balancing_mask = balMask;
            m_telemetry.balancing_active = (balMask != 0);

            // 2. Protection / Alarm Status Flags (bytes 16..17)
            if (dataLen >= 18) {
                uint16_t errMask = (p[16] << 8) | p[17];
                m_telemetry.raw_errors = errMask;
                if (errMask == 0) {
                    m_telemetry.errors_str = "OK";
                } else {
                    String errs = "";
                    if (errMask & (1 << 0)) errs += "Перенапруга комірки (COVP); ";
                    if (errMask & (1 << 1)) errs += "Знижена напруга комірки (CUVP); ";
                    if (errMask & (1 << 2)) errs += "Перенапруга батареї (POVP); ";
                    if (errMask & (1 << 3)) errs += "Знижена напруга батареї (PUVP); ";
                    if (errMask & (1 << 4)) errs += "Перегрів заряду (CHGOT); ";
                    if (errMask & (1 << 5)) errs += "Переохолодження заряду (CHGUT); ";
                    if (errMask & (1 << 6)) errs += "Перегрів розряду (DSGOT); ";
                    if (errMask & (1 << 7)) errs += "Переохолодження розряду (DSGUT); ";
                    if (errMask & (1 << 8)) errs += "Струм заряду (CHGOC); ";
                    if (errMask & (1 << 9)) errs += "Струм розряду (DSGOC); ";
                    if (errMask & (1 << 10)) errs += "Коротке замикання (SC); ";
                    if (errMask & (1 << 11)) errs += "Помилка AFE/IC; ";
                    if (errMask & (1 << 12)) errs += "Блокування MOS; ";
                    errs.trim();
                    m_telemetry.errors_str = errs;
                }
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
                char c = (char)m_rxBuffer[4 + i];
                if (c >= 32 && c <= 126) name += c;
            }
            name.trim();
            if (name.length() > 0) {
                m_telemetry.device_name = name;
                Serial.printf("[BLE] Updated JBD Device Name from 0x05: '%s'\n", name.c_str());
            }
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
    NimBLERemoteCharacteristic* target = m_pJkWriteChar ? m_pJkWriteChar : m_pJkNotifyChar;
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
    } else if (m_telemetry.bms_type == "Daly-BMS") {
        uint8_t payload[8] = { (uint8_t)(enable ? 0x01 : 0x00), 0, 0, 0, 0, 0, 0, 0 };
        sendDalyCommand(0xDA, payload);
        m_telemetry.switch_charging = enable;
        return true;
    } else if (m_telemetry.bms_type == "Ant-BMS") {
        bool ok = sendAntSwitchCommand(0x01, enable ? 1 : 0);
        if (ok) m_telemetry.switch_charging = enable;
        return ok;
    } else if (m_telemetry.bms_type == "Redodo-BMS") {
        return false;
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
    } else if (m_telemetry.bms_type == "Daly-BMS") {
        uint8_t payload[8] = { (uint8_t)(enable ? 0x01 : 0x00), 0, 0, 0, 0, 0, 0, 0 };
        sendDalyCommand(0xD9, payload);
        m_telemetry.switch_discharging = enable;
        return true;
    } else if (m_telemetry.bms_type == "Ant-BMS") {
        bool ok = sendAntSwitchCommand(0x02, enable ? 1 : 0);
        if (ok) m_telemetry.switch_discharging = enable;
        return ok;
    } else if (m_telemetry.bms_type == "Redodo-BMS") {
        return false;
    } else {
        bool ok = writeJkRegister(30, enable ? 1 : 0, 4); // JK02 Reg 30: Discharge Switch
        if (ok) m_telemetry.switch_discharging = enable;
        return ok;
    }
}

bool BmsBleClient::setBalancer(bool enable) {
    if (m_telemetry.bms_type == "JBD-BMS" || m_telemetry.bms_type == "Daly-BMS" || m_telemetry.bms_type == "Redodo-BMS") {
        return false; // Autonomous balancing
    } else if (m_telemetry.bms_type == "Ant-BMS") {
        bool ok = sendAntSwitchCommand(0x03, enable ? 1 : 0);
        if (ok) m_telemetry.switch_balancer = enable;
        return ok;
    } else {
        bool ok = writeJkRegister(31, enable ? 1 : 0, 4); // JK02 Reg 31: Active Balancer Switch
        if (ok) m_telemetry.switch_balancer = enable;
        return ok;
    }
}

// ======================== Daly Implementation ========================

uint8_t BmsBleClient::calcDalyChecksum(const uint8_t* buf, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += buf[i];
    }
    return sum;
}

void BmsBleClient::sendDalyCommand(uint8_t cmdId, const uint8_t* payload) {
    if (!m_isConnected) return;
    NimBLERemoteCharacteristic* writeChar = m_pDalyWriteChar ? m_pDalyWriteChar : m_pDalyNotifyChar;
    if (!writeChar) return;

    uint8_t frame[13];
    frame[0] = 0xA5;
    frame[1] = 0x40; // Host address
    frame[2] = cmdId;
    frame[3] = 0x08; // Length
    if (payload) {
        memcpy(&frame[4], payload, 8);
    } else {
        memset(&frame[4], 0, 8);
    }
    frame[12] = calcDalyChecksum(frame, 12);

    writeChar->writeValue(frame, 13, m_dalyWriteWithResponse);
}

void BmsBleClient::handleDalyPacket(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    size_t i = 0;
    while (i < m_rxBuffer.size()) {
        if (m_rxBuffer[i] != 0xA5) {
            i++;
            continue;
        }

        if (m_rxBuffer.size() - i < 13) {
            break;
        }

        uint8_t dataLen = m_rxBuffer[i + 3];
        if (dataLen != 0x08) {
            i++;
            continue;
        }

        uint8_t expectedCrc = m_rxBuffer[i + 12];
        uint8_t calcCrc = calcDalyChecksum(&m_rxBuffer[i], 12);

        if (calcCrc == expectedCrc) {
            uint8_t cmdId = m_rxBuffer[i + 2];
            decodeDalyTelemetry(cmdId, &m_rxBuffer[i + 4]);
            i += 13;
        } else {
            i++;
        }
    }

    if (i > 0) {
        m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + i);
    }
    if (m_rxBuffer.size() > 256) {
        m_rxBuffer.clear();
    }
}

void BmsBleClient::decodeDalyTelemetry(uint8_t cmdId, const uint8_t* data) {
    m_telemetry.connected = true;
    m_telemetry.bms_type = "Daly-BMS";
    m_telemetry.last_update = millis();

    switch (cmdId) {
        case 0x90: { // Total Voltage, Current, SoC
            uint16_t vRaw = (uint16_t(data[0]) << 8) | data[1];
            m_telemetry.total_voltage = (float)vRaw * 0.1f;

            int32_t iRaw = (int32_t(data[4]) << 8) | data[5];
            m_telemetry.current = (float)(iRaw - 30000) * 0.1f;
            m_telemetry.power = m_telemetry.total_voltage * m_telemetry.current;

            uint16_t socRaw = (uint16_t(data[6]) << 8) | data[7];
            m_telemetry.soc = (float)socRaw * 0.1f;
            break;
        }

        case 0x91: { // Min/Max Cell V
            uint16_t maxMv = (uint16_t(data[0]) << 8) | data[1];
            m_telemetry.max_cell_idx = data[2];
            uint16_t minMv = (uint16_t(data[3]) << 8) | data[4];
            m_telemetry.min_cell_idx = data[5];

            m_telemetry.max_cell_v = (float)maxMv * 0.001f;
            m_telemetry.min_cell_v = (float)minMv * 0.001f;
            m_telemetry.delta_cell_v = m_telemetry.max_cell_v - m_telemetry.min_cell_v;
            break;
        }

        case 0x92: { // Temperature Extremes
            int8_t maxT = (int8_t)data[0] - 40;
            int8_t minT = (int8_t)data[2] - 40;
            m_telemetry.temp_sensor1 = (float)maxT;
            m_telemetry.temp_sensor2 = (float)minT;
            break;
        }

        case 0x93: { // MOS Status & Residual Capacity
            m_telemetry.switch_charging = (data[1] == 1);
            m_telemetry.switch_discharging = (data[2] == 1);
            uint32_t remMah = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | data[7];
            m_telemetry.capacity_remain = (float)remMah * 0.001f;
            break;
        }

        case 0x94: { // Cells & Sensor Count, Cycle Count
            uint8_t cells = data[0];
            if (cells >= 1 && cells <= 32) {
                m_telemetry.cell_count = cells;
            }
            m_telemetry.cycle_count = (uint16_t(data[5]) << 8) | data[6];
            break;
        }

        case 0x95: { // Cell Voltages (3 cells per frame)
            uint8_t frameNum = data[0];
            if (frameNum >= 1 && frameNum <= 11) {
                uint8_t baseIdx = (frameNum - 1) * 3;
                uint16_t v1 = (uint16_t(data[1]) << 8) | data[2];
                uint16_t v2 = (uint16_t(data[3]) << 8) | data[4];
                uint16_t v3 = (uint16_t(data[5]) << 8) | data[6];

                if (baseIdx < 32 && (baseIdx < m_telemetry.cell_count || m_telemetry.cell_count == 0)) {
                    if (v1 > 0) m_telemetry.cell_voltages[baseIdx] = (float)v1 * 0.001f;
                }
                if (baseIdx + 1 < 32 && (baseIdx + 1 < m_telemetry.cell_count || m_telemetry.cell_count == 0)) {
                    if (v2 > 0) m_telemetry.cell_voltages[baseIdx + 1] = (float)v2 * 0.001f;
                }
                if (baseIdx + 2 < 32 && (baseIdx + 2 < m_telemetry.cell_count || m_telemetry.cell_count == 0)) {
                    if (v3 > 0) m_telemetry.cell_voltages[baseIdx + 2] = (float)v3 * 0.001f;
                }

                // Recalculate min/max/delta
                float minV = 999.0f;
                float maxV = 0.0f;
                uint8_t minIdx = 1;
                uint8_t maxIdx = 1;
                for (uint8_t c = 0; c < m_telemetry.cell_count && c < 32; ++c) {
                    float cv = m_telemetry.cell_voltages[c];
                    if (cv > 0.5f) {
                        if (cv < minV) { minV = cv; minIdx = c + 1; }
                        if (cv > maxV) { maxV = cv; maxIdx = c + 1; }
                    }
                }
                if (maxV > 0.0f) {
                    m_telemetry.max_cell_v = maxV;
                    m_telemetry.max_cell_idx = maxIdx;
                    m_telemetry.min_cell_v = (minV < 900.0f) ? minV : 0.0f;
                    m_telemetry.min_cell_idx = minIdx;
                    m_telemetry.delta_cell_v = m_telemetry.max_cell_v - m_telemetry.min_cell_v;
                }
            }
            break;
        }

        case 0x96: { // Temperature Sensors
            uint8_t frameNum = data[0];
            if (frameNum == 1) {
                if (data[1] != 0) m_telemetry.temp_sensor1 = (int8_t)data[1] - 40;
                if (data[2] != 0) m_telemetry.temp_sensor2 = (int8_t)data[2] - 40;
                if (data[3] != 0) m_telemetry.temp_mos = (int8_t)data[3] - 40;
            }
            break;
        }

        case 0x97: { // Balancing status
            uint32_t balMask = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
            m_telemetry.balancing_active = (balMask != 0);
            m_telemetry.balancing_mask = balMask;
            m_telemetry.switch_balancer = (balMask != 0);
            break;
        }

        case 0x98: { // Fault / Failure Status (8 bytes)
            m_telemetry.raw_errors = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
            bool hasError = false;
            String errStr = "";

            if (data[0] & 0x01) { errStr += "Cell OV L1; "; hasError = true; }
            if (data[0] & 0x02) { errStr += "Cell OV L2; "; hasError = true; }
            if (data[0] & 0x04) { errStr += "Cell UV L1; "; hasError = true; }
            if (data[0] & 0x08) { errStr += "Cell UV L2; "; hasError = true; }
            if (data[0] & 0x10) { errStr += "Pack OV L1; "; hasError = true; }
            if (data[0] & 0x20) { errStr += "Pack OV L2; "; hasError = true; }
            if (data[0] & 0x40) { errStr += "Pack UV L1; "; hasError = true; }
            if (data[0] & 0x80) { errStr += "Pack UV L2; "; hasError = true; }

            if (data[1] & 0x01) { errStr += "Chg OT L1; "; hasError = true; }
            if (data[1] & 0x02) { errStr += "Chg OT L2; "; hasError = true; }
            if (data[1] & 0x04) { errStr += "Chg UT L1; "; hasError = true; }
            if (data[1] & 0x08) { errStr += "Chg UT L2; "; hasError = true; }
            if (data[1] & 0x10) { errStr += "Dsg OT L1; "; hasError = true; }
            if (data[1] & 0x20) { errStr += "Dsg OT L2; "; hasError = true; }
            if (data[1] & 0x40) { errStr += "Dsg UT L1; "; hasError = true; }
            if (data[1] & 0x80) { errStr += "Dsg UT L2; "; hasError = true; }

            if (data[2] & 0x01) { errStr += "Chg OC L1; "; hasError = true; }
            if (data[2] & 0x02) { errStr += "Chg OC L2; "; hasError = true; }
            if (data[2] & 0x04) { errStr += "Dsg OC L1; "; hasError = true; }
            if (data[2] & 0x08) { errStr += "Dsg OC L2; "; hasError = true; }
            if (data[2] & 0x10) { errStr += "High SOC; "; hasError = true; }
            if (data[2] & 0x20) { errStr += "Low SOC; "; hasError = true; }
            if (data[2] & 0x40) { errStr += "Cell Diff L1; "; hasError = true; }
            if (data[2] & 0x80) { errStr += "Cell Diff L2; "; hasError = true; }

            if (data[3] & 0x01) { errStr += "MOS Over-temp; "; hasError = true; }
            if (data[3] & 0x02) { errStr += "Temp Sensor Fault; "; hasError = true; }
            if (data[3] & 0x04) { errStr += "Cell Sampling Fault; "; hasError = true; }
            if (data[3] & 0x08) { errStr += "MOS Fault; "; hasError = true; }
            if (data[3] & 0x10) { errStr += "Short Circuit; "; hasError = true; }

            if (hasError) {
                m_telemetry.errors_str = errStr;
            } else {
                m_telemetry.errors_str = "OK (Без помилок)";
            }
            break;
        }

        case 0x21:
        case 0x51: { // Rated Capacity
            uint32_t nomMah = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
            if (nomMah > 0) {
                m_telemetry.capacity_nominal = (float)nomMah * 0.001f;
                Serial.printf("[BLE] Daly Rated Capacity: %.1f Ah\n", m_telemetry.capacity_nominal);
            }
            break;
        }

        default:
            break;
    }
}

// ======================== Ant BMS Implementation ========================

void BmsBleClient::sendAntPollRequest() {
    if (!m_isConnected) return;
    NimBLERemoteCharacteristic* writeChar = m_pAntWriteChar ? m_pAntWriteChar : m_pAntNotifyChar;
    if (!writeChar) return;

    static const uint8_t pollCmd[2] = { 0xDB, 0xDB };
    writeChar->writeValue(pollCmd, sizeof(pollCmd), !writeChar->canWriteNoResponse());
}

bool BmsBleClient::sendAntSwitchCommand(uint8_t cmd, uint8_t state) {
    if (!m_isConnected) return false;
    NimBLERemoteCharacteristic* writeChar = m_pAntWriteChar ? m_pAntWriteChar : m_pAntNotifyChar;
    if (!writeChar) return false;

    // Format: 0xA5 0xA5 <cmd> <state> 0x00 0xAA
    uint8_t frame[6] = { 0xA5, 0xA5, cmd, state, 0x00, 0xAA };
    return writeChar->writeValue(frame, sizeof(frame), !writeChar->canWriteNoResponse());
}

void BmsBleClient::handleAntPacket(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    // Prevent buffer runaway
    if (m_rxBuffer.size() > 512) {
        m_rxBuffer.clear();
        return;
    }

    // Look for Ant Legacy 140-byte frame: AA 55 AA FF
    while (m_rxBuffer.size() >= 4) {
        if (m_rxBuffer[0] == 0xAA && m_rxBuffer[1] == 0x55 && m_rxBuffer[2] == 0xAA && m_rxBuffer[3] == 0xFF) {
            if (m_rxBuffer.size() < 140) {
                // Wait for complete 140B frame
                return;
            }

            // Verify checksum: sum of bytes 4..137
            uint16_t calcCrc = 0;
            for (size_t i = 4; i < 138; ++i) {
                calcCrc += m_rxBuffer[i];
            }
            uint16_t expectedCrc = ((uint16_t)m_rxBuffer[138] << 8) | m_rxBuffer[139];

            if (calcCrc == expectedCrc) {
                decodeAntLegacyTelemetry(m_rxBuffer.data());
                m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + 140);
                continue;
            } else {
                Serial.printf("[ANT] Checksum mismatch (calc 0x%04X != exp 0x%04X)\n", calcCrc, expectedCrc);
                m_rxBuffer.erase(m_rxBuffer.begin());
            }
        } else {
            m_rxBuffer.erase(m_rxBuffer.begin());
        }
    }
}

void BmsBleClient::decodeAntLegacyTelemetry(const uint8_t* frame) {
    if (!frame) return;

    // Effective cell count (Byte 123)
    uint8_t cells = frame[123];
    if (cells < 1 || cells > 32) {
        // Fallback: count non-zero cell voltages
        cells = 0;
        for (uint8_t i = 0; i < 32; ++i) {
            uint16_t mv = ((uint16_t)frame[4 + i * 2] << 8) | frame[4 + i * 2 + 1];
            if (mv > 500) cells++;
        }
        if (cells == 0) cells = m_config.cell_count;
    }
    m_telemetry.cell_count = cells;

    // Cell Voltages 1..32 (bytes 4..67, mV big-endian)
    float sumV = 0.0f;
    float minV = 999.0f;
    float maxV = 0.0f;
    uint8_t minIdx = 1;
    uint8_t maxIdx = 1;

    for (uint8_t i = 0; i < cells && i < 32; ++i) {
        uint16_t mv = ((uint16_t)frame[4 + i * 2] << 8) | frame[4 + i * 2 + 1];
        float v = (float)mv * 0.001f;
        m_telemetry.cell_voltages[i] = v;
        sumV += v;
        if (v < minV) { minV = v; minIdx = i + 1; }
        if (v > maxV) { maxV = v; maxIdx = i + 1; }
    }
    for (uint8_t i = cells; i < 32; ++i) {
        m_telemetry.cell_voltages[i] = 0.0f;
    }

    m_telemetry.total_voltage = sumV;
    m_telemetry.min_cell_v = minV;
    m_telemetry.max_cell_v = maxV;
    m_telemetry.delta_cell_v = (maxV > minV) ? (maxV - minV) : 0.0f;
    m_telemetry.min_cell_idx = minIdx;
    m_telemetry.max_cell_idx = maxIdx;

    // Current (bytes 70..73, int32 BE, unit 0.1A)
    int32_t cur01a = (int32_t)(((uint32_t)frame[70] << 24) |
                               ((uint32_t)frame[71] << 16) |
                               ((uint32_t)frame[72] << 8)  |
                               ((uint32_t)frame[73]));
    m_telemetry.current = (float)cur01a * 0.1f;
    m_telemetry.power = m_telemetry.total_voltage * m_telemetry.current;

    // SOC (byte 74, %)
    m_telemetry.soc = (float)frame[74];

    // Total Nominal Capacity (bytes 75..78, uint32 BE, mAh)
    uint32_t totalMah = ((uint32_t)frame[75] << 24) |
                        ((uint32_t)frame[76] << 16) |
                        ((uint32_t)frame[77] << 8)  |
                        ((uint32_t)frame[78]);
    m_telemetry.capacity_nominal = (float)totalMah * 0.001f;

    // Remaining Capacity (bytes 79..82, uint32 BE, mAh)
    uint32_t remainMah = ((uint32_t)frame[79] << 24) |
                         ((uint32_t)frame[80] << 16) |
                         ((uint32_t)frame[81] << 8)  |
                         ((uint32_t)frame[82]);
    m_telemetry.capacity_remain = (float)remainMah * 0.001f;

    // Cycle Capacity (bytes 83..86, uint32 BE, mAh)
    uint32_t cycleMah = ((uint32_t)frame[83] << 24) |
                        ((uint32_t)frame[84] << 16) |
                        ((uint32_t)frame[85] << 8)  |
                        ((uint32_t)frame[86]);
    if (totalMah > 0) {
        m_telemetry.cycle_count = cycleMah / totalMah;
    }

    // Temperatures (°C signed int16 BE): MOS (91..92), T1 (93..94), T2 (95..96)
    int16_t tMos = (int16_t)(((uint16_t)frame[91] << 8) | frame[92]);
    int16_t t1   = (int16_t)(((uint16_t)frame[93] << 8) | frame[94]);
    int16_t t2   = (int16_t)(((uint16_t)frame[95] << 8) | frame[96]);
    m_telemetry.temp_mos     = (float)tMos;
    m_telemetry.temp_sensor1 = (float)t1;
    m_telemetry.temp_sensor2 = (float)t2;

    // Switches
    m_telemetry.switch_charging    = (frame[101] != 0 || frame[103] != 0);
    m_telemetry.switch_discharging = (frame[102] != 0 || frame[104] != 0);
    m_telemetry.switch_balancer    = (frame[105] != 0);
    m_telemetry.balancing_active   = (frame[105] != 0);

    m_telemetry.errors_str = "OK (Без помилок)";
    m_telemetry.last_update = millis();
}

// ======================== Redodo / LiTime Implementation ========================

void BmsBleClient::sendRedodoPollRequest() {
    if (!m_isConnected) return;
    NimBLERemoteCharacteristic* writeChar = m_pRedodoWriteChar ? m_pRedodoWriteChar : m_pRedodoNotifyChar;
    if (!writeChar) return;

    static const uint8_t pollCmd[8] = { 0x00, 0x00, 0x04, 0x01, 0x13, 0x55, 0xAA, 0x17 };
    writeChar->writeValue(pollCmd, sizeof(pollCmd), !writeChar->canWriteNoResponse());
}

void BmsBleClient::handleRedodoPacket(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    m_rxBuffer.insert(m_rxBuffer.end(), data, data + len);

    // Prevent buffer runaway
    if (m_rxBuffer.size() > 512) {
        m_rxBuffer.clear();
        return;
    }

    // Look for Redodo 105-byte frame: 00 00 61 01 93 55 AA 00
    while (m_rxBuffer.size() >= 8) {
        if (m_rxBuffer[0] == 0x00 && m_rxBuffer[1] == 0x00 && m_rxBuffer[2] == 0x61 && m_rxBuffer[3] == 0x01 &&
            m_rxBuffer[4] == 0x93 && m_rxBuffer[5] == 0x55 && m_rxBuffer[6] == 0xAA) {
            if (m_rxBuffer.size() < 105) {
                // Wait for complete 105B frame
                return;
            }

            // Verify checksum: sum of bytes 0..103 == frame[104]
            uint8_t csum = 0;
            for (size_t i = 0; i < 104; ++i) {
                csum += m_rxBuffer[i];
            }

            if (csum == m_rxBuffer[104]) {
                decodeRedodoTelemetry(m_rxBuffer.data());
                m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + 105);
                continue;
            } else {
                Serial.printf("[REDODO] Checksum mismatch (calc 0x%02X != exp 0x%02X)\n", csum, m_rxBuffer[104]);
                m_rxBuffer.erase(m_rxBuffer.begin());
            }
        } else {
            m_rxBuffer.erase(m_rxBuffer.begin());
        }
    }
}

void BmsBleClient::decodeRedodoTelemetry(const uint8_t* frame) {
    if (!frame) return;

    // Terminal Voltage (Offset 8..11, uint32 LE, mV)
    uint32_t termMv = 0;
    memcpy(&termMv, &frame[8], 4);
    m_telemetry.total_voltage = (float)termMv * 0.001f;

    // Cell count: count valid cell voltages from Offset 16..47 (16 * uint16 LE, mV)
    uint8_t validCells = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        uint16_t mv = 0;
        memcpy(&mv, &frame[16 + i * 2], 2);
        if (mv > 500) validCells++;
    }
    if (validCells == 0) validCells = m_config.cell_count;
    m_telemetry.cell_count = validCells;

    float minV = 999.0f;
    float maxV = 0.0f;
    uint8_t minIdx = 1;
    uint8_t maxIdx = 1;

    for (uint8_t i = 0; i < validCells && i < 32; ++i) {
        uint16_t mv = 0;
        memcpy(&mv, &frame[16 + i * 2], 2);
        float v = (float)mv * 0.001f;
        m_telemetry.cell_voltages[i] = v;
        if (v < minV) { minV = v; minIdx = i + 1; }
        if (v > maxV) { maxV = v; maxIdx = i + 1; }
    }
    for (uint8_t i = validCells; i < 32; ++i) {
        m_telemetry.cell_voltages[i] = 0.0f;
    }

    m_telemetry.min_cell_v = minV;
    m_telemetry.max_cell_v = maxV;
    m_telemetry.delta_cell_v = (maxV > minV) ? (maxV - minV) : 0.0f;
    m_telemetry.min_cell_idx = minIdx;
    m_telemetry.max_cell_idx = maxIdx;

    // Current (Offset 48..51, int32 LE, mA)
    int32_t currentMa = 0;
    memcpy(&currentMa, &frame[48], 4);
    m_telemetry.current = (float)currentMa * 0.001f;
    m_telemetry.power = m_telemetry.total_voltage * m_telemetry.current;

    // Temperatures in °C (Offset 52..57, 3 * int16 LE)
    int16_t tCells = 0, tMos = 0, tAmb = 0;
    memcpy(&tCells, &frame[52], 2);
    memcpy(&tMos,   &frame[54], 2);
    memcpy(&tAmb,   &frame[56], 2);
    m_telemetry.temp_sensor1 = (float)tCells;
    m_telemetry.temp_mos     = (float)tMos;
    m_telemetry.temp_sensor2 = (float)tAmb;

    // Cycle count (Offset 58..59, uint16 LE)
    uint16_t cycles = 0;
    memcpy(&cycles, &frame[58], 2);
    m_telemetry.cycle_count = cycles;

    // Remaining capacity (Offset 62..63, uint16 LE, unit 0.01 Ah)
    uint16_t remCap001Ah = 0;
    memcpy(&remCap001Ah, &frame[62], 2);
    m_telemetry.capacity_remain = (float)remCap001Ah * 0.01f;

    // Full capacity (Offset 64..65, uint16 LE, unit 0.01 Ah)
    uint16_t fullCap001Ah = 0;
    memcpy(&fullCap001Ah, &frame[64], 2);
    m_telemetry.capacity_nominal = (float)fullCap001Ah * 0.01f;

    // Protection status (Offset 66..67, uint16 LE)
    uint16_t prot = 0;
    memcpy(&prot, &frame[66], 2);
    m_telemetry.raw_errors = prot;
    if (prot != 0) {
        m_telemetry.errors_str = "Protection: 0x" + String(prot, HEX);
    } else {
        m_telemetry.errors_str = "OK (Без помилок)";
    }

    // Balancing mask (Offset 68..71, uint32 LE)
    uint32_t balMask = 0;
    memcpy(&balMask, &frame[68], 4);
    m_telemetry.balancing_mask = balMask;
    m_telemetry.balancing_active = (balMask != 0);
    m_telemetry.switch_balancer = (balMask != 0);

    // Switches: default active unless protection triggered
    m_telemetry.switch_charging = (prot == 0);
    m_telemetry.switch_discharging = (prot == 0);

    // SOC (Offset 90..91, uint16 LE, %)
    uint16_t soc = 0;
    memcpy(&soc, &frame[90], 2);
    m_telemetry.soc = (float)soc;

    m_telemetry.last_update = millis();
}
