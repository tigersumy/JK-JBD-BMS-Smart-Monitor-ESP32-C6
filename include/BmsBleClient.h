#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include "BmsProtocol.h"
#include "Config.h"

struct BleDiscoveredDevice {
    String name;
    String address;
    int rssi;
    String bms_type;
    uint8_t addr_type = 0;
};

class BmsBleClient {
public:
    BmsBleClient();
    bool init();
    void loop();

    void setTargetConfig(const AppConfig& cfg);
    BmsTelemetry getTelemetry() {
        if (m_mutex && xSemaphoreTake(m_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            BmsTelemetry copy = m_telemetry;
            xSemaphoreGive(m_mutex);
            return copy;
        }
        return m_telemetry;
    }
    const AppConfig& getConfig() const { return m_config; }

    // Control functions
    bool setCharging(bool enable);
    bool setDischarging(bool enable);
    bool setBalancer(bool enable);
    void disconnect();
    void reconnect();

    // Scanner functions
    void startScan(uint32_t durationSeconds = 5);
    String performScanSync(uint32_t durationSeconds = 3);
    bool isScanning() const { return m_isScanning; }
    std::vector<BleDiscoveredDevice> getDiscoveredDevices();
    String getDiscoveredDevicesJson();

private:
    SemaphoreHandle_t m_mutex = nullptr;
    AppConfig m_config;
    BmsTelemetry m_telemetry;

    NimBLEClient* m_pClient = nullptr;
    NimBLERemoteCharacteristic* m_pJbdNotifyChar = nullptr;
    NimBLERemoteCharacteristic* m_pJbdWriteChar  = nullptr;
    NimBLERemoteCharacteristic* m_pJkNotifyChar  = nullptr;
    NimBLERemoteCharacteristic* m_pJkWriteChar   = nullptr;

    bool m_isConnected = false;
    bool m_isScanning  = false;
    uint32_t m_lastPollTime = 0;
    uint32_t m_lastScanStartTime = 0;
    uint32_t m_lastConnectAttempt = 0;
    uint8_t  m_pollStep = 0;

    std::vector<BleDiscoveredDevice> m_discoveredDevices;
    std::vector<uint8_t> m_rxBuffer;

    bool connectToDevice(const NimBLEAddress& address, const String& name, uint8_t forcedType = BMS_TYPE_AUTO);

    // JBD protocol handlers
    void handleJbdPacket(const uint8_t* data, size_t len);
    void sendJbdCommand(uint8_t cmd, uint8_t reg, const uint8_t* payload = nullptr, uint8_t len = 0);
    bool writeJbdFetState(uint8_t newFetMask);

    // JK protocol handlers
    static uint8_t calcJkCrc(const uint8_t* data, size_t len);
    std::vector<uint8_t> buildJkFrame(uint8_t address, uint32_t value, uint8_t length);
    void handleJkPacket(const uint8_t* data, size_t len);
    void decodeJkCellInfo(const std::vector<uint8_t>& data);
    void decodeJkSettings(const std::vector<uint8_t>& data);
    void sendJkPollRequest();
    bool writeJkRegister(uint8_t reg, uint32_t value, uint8_t length = 4);

    // BLE Callbacks
    friend class BmsScanCallbacks;
    friend class BmsClientCallbacks;
    friend class BmsNotifyCallbacks;
};
