#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "BmsBleClient.h"
#include "WebDashboard.h"

// DNS Server for Captive Portal
static const byte DNS_PORT = 53;
static DNSServer dnsServer;

// Web Server
static WebServer server(80);

// Global objects
static AppConfig g_config;
static BmsBleClient g_bleClient;

static bool g_isApMode = false;
static uint32_t g_lastBleLoop = 0;
static uint32_t g_wifiConnectStartTime = 0;

void setupWifi() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.setSleep(false);
    delay(100);

    if (g_config.wifi_ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        Serial.printf("[WiFi] Connecting to %s...\n", g_config.wifi_ssid.c_str());
        WiFi.begin(g_config.wifi_ssid.c_str(), g_config.wifi_pass.c_str());

        g_wifiConnectStartTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - g_wifiConnectStartTime < 10000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();
    }

    if (WiFi.status() == WL_CONNECTED) {
        g_isApMode = false;
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin("bms-monitor")) {
            Serial.println("[MDNS] Responder started: http://bms-monitor.local");
            MDNS.addService("http", "tcp", 80);
        }
    } else {
        g_isApMode = true;
        Serial.println("[WiFi] Starting Fallback Access Point (AP Mode)...");
        WiFi.mode(WIFI_AP);
        IPAddress apIP(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.softAPConfig(apIP, gateway, subnet);
        bool apOk = WiFi.softAP("BMS-Monitor-AP", nullptr, 1, 0, 4);
        delay(200);
        Serial.printf("[WiFi] AP status: %s, AP IP address: %s\n", apOk ? "OK" : "ERR", WiFi.softAPIP().toString().c_str());

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(DNS_PORT, "*", apIP);
        if (MDNS.begin("bms-monitor")) {
            MDNS.addService("http", "tcp", 80);
        }
    }
}

void setupHttpRoutes() {
    // Web Pages
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    server.on("/setup", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
    });

    server.on("/settings", HTTP_GET, []() {
        server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
    });

    server.on("/favicon.svg", HTTP_GET, []() {
        server.send_P(200, "image/svg+xml", FAVICON_SVG);
    });

    server.on("/favicon.ico", HTTP_GET, []() {
        server.send_P(200, "image/svg+xml", FAVICON_SVG);
    });

    // Captive Portal Redirects
    server.on("/generate_204", HTTP_GET, []() {
        server.sendHeader("Location", "/setup", true);
        server.send(302, "text/plain", "");
    });
    server.on("/hotspot-detect.html", HTTP_GET, []() {
        server.sendHeader("Location", "/setup", true);
        server.send(302, "text/plain", "");
    });
    server.on("/canonical.html", HTTP_GET, []() {
        server.sendHeader("Location", "/setup", true);
        server.send(302, "text/plain", "");
    });

    // API: Telemetry
    server.on("/api/data", HTTP_GET, []() {
        const BmsTelemetry& telem = g_bleClient.getTelemetry();

        JsonDocument doc;
        doc["connected"] = telem.connected;
        doc["bms_type"] = telem.bms_type;
        doc["name"] = telem.device_name.length() > 0 ? telem.device_name : g_config.bms_name;
        doc["mac"] = telem.mac_address.length() > 0 ? telem.mac_address : g_config.bms_mac;
        doc["last_update"] = telem.last_update;

        // KPI
        doc["total_voltage"] = telem.total_voltage;
        doc["current"] = telem.current;
        doc["power"] = telem.power;
        doc["soc"] = telem.soc;
        doc["capacity_remain"] = telem.capacity_remain;
        doc["capacity_nominal"] = telem.capacity_nominal;
        doc["cycle_count"] = telem.cycle_count;

        // Cells
        uint8_t count = telem.cell_count > 0 ? telem.cell_count : g_config.cell_count;
        if (count == 0) count = 8;
        doc["cell_count"] = count;

        JsonArray cellsArr = doc["cells"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            cellsArr.add(telem.cell_voltages[i]);
        }

        doc["min_cell_idx"] = telem.min_cell_idx;
        doc["max_cell_idx"] = telem.max_cell_idx;
        doc["min_cell_v"] = telem.min_cell_v;
        doc["max_cell_v"] = telem.max_cell_v;
        doc["delta_cell_v"] = telem.delta_cell_v;

        // Temps
        doc["temp_mos"] = telem.temp_mos;
        doc["temp_sensor1"] = telem.temp_sensor1;
        doc["temp_sensor2"] = telem.temp_sensor2;

        // Balancer & Switches
        doc["balancing_active"] = telem.balancing_active;
        doc["balancing_current"] = telem.balancing_current;
        doc["switch_charging"] = telem.switch_charging;
        doc["switch_discharging"] = telem.switch_discharging;
        doc["switch_balancer"] = telem.switch_balancer;

        doc["errors"] = telem.errors_str;
        doc["heap_free"] = ESP.getFreeHeap() / 1024;
        doc["wifi_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
        doc["ip"] = g_isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json; charset=utf-8", response);
    });

    // API: Switches Control
    server.on("/api/switch", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing body\"}");
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }

        String sw = doc["switch"].as<String>();
        bool state = doc["state"].as<bool>();
        bool ok = false;

        if (sw == "charging") {
            ok = g_bleClient.setCharging(state);
        } else if (sw == "discharging") {
            ok = g_bleClient.setDischarging(state);
        } else if (sw == "balancer") {
            ok = g_bleClient.setBalancer(state);
        }

        JsonDocument resp;
        resp["status"] = ok ? "ok" : "failed";
        resp["switch"] = sw;
        resp["state"] = state;

        String resStr;
        serializeJson(resp, resStr);
        server.send(200, "application/json", resStr);
    });

    // API: Cell count override
    server.on("/api/set-cells", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"status\":\"error\"}");
            return;
        }
        JsonDocument doc;
        deserializeJson(doc, server.arg("plain"));
        uint8_t cells = doc["cells"].as<uint8_t>();
        if (cells >= 2 && cells <= 32) {
            g_config.cell_count = cells;
            ConfigManager::save(g_config);
            g_bleClient.setTargetConfig(g_config);
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(400, "application/json", "{\"status\":\"invalid_cell_count\"}");
        }
    });

    // API: Wi-Fi Scanner
    server.on("/api/scan-wifi", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; ++i) {
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"] = WiFi.SSID(i);
            obj["rssi"] = WiFi.RSSI(i);
            obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    // API: BLE Scanner
    server.on("/api/scan-ble", HTTP_GET, []() {
        String json = g_bleClient.performScanSync(6);
        server.send(200, "application/json", json);
    });

    // API: Config Management
    server.on("/api/config", HTTP_GET, []() {
        JsonDocument doc;
        doc["ssid"] = g_config.wifi_ssid;
        doc["mac"] = g_config.bms_mac;
        doc["name"] = g_config.bms_name;
        doc["type"] = g_config.bms_type;
        doc["pin"] = g_config.bms_pin;
        doc["cells"] = g_config.cell_count;

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/save-config", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"status\":\"error\"}");
            return;
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"status\":\"invalid_json\"}");
            return;
        }

        if (!doc["ssid"].isNull()) g_config.wifi_ssid = doc["ssid"].as<String>();
        if (!doc["pass"].isNull()) {
            String p = doc["pass"].as<String>();
            if (p.length() > 0) g_config.wifi_pass = p;
        }
        if (!doc["mac"].isNull()) g_config.bms_mac = doc["mac"].as<String>();
        if (!doc["name"].isNull()) g_config.bms_name = doc["name"].as<String>();
        if (!doc["type"].isNull()) g_config.bms_type = doc["type"].as<uint8_t>();
        if (!doc["pin"].isNull()) g_config.bms_pin = doc["pin"].as<String>();
        if (!doc["cells"].isNull()) g_config.cell_count = doc["cells"].as<uint8_t>();

        ConfigManager::save(g_config);
        g_bleClient.setTargetConfig(g_config);

        server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Saved. Applying...\"}");
        delay(500);

        // Disconnect and reconnect BLE with new config
        // If Wi-Fi SSID was modified, restart to connect to new Wi-Fi
        Serial.println("[Config] New settings saved. Restarting ESP32...");
        delay(500);
        ESP.restart();
    });

    server.begin();
    Serial.println("[HTTP] WebServer started on port 80");
}

#ifndef BUTTON_PIN
#define BUTTON_PIN 9
#endif

static uint32_t s_btnPressStartTime = 0;
static bool s_btnWasPressed = false;
static bool s_factoryResetTriggered = false;

void handleButton() {
    bool isPressed = (digitalRead(BUTTON_PIN) == LOW);

    if (isPressed) {
        if (!s_btnWasPressed) {
            s_btnWasPressed = true;
            s_btnPressStartTime = millis();
            s_factoryResetTriggered = false;
            Serial.println("[Button] GPIO 9 pressed...");
        } else {
            uint32_t duration = millis() - s_btnPressStartTime;
            if (duration >= 3000 && !s_factoryResetTriggered) {
                s_factoryResetTriggered = true;
                Serial.println("\n=======================================================");
                Serial.println("  [Button] LONG PRESS (>3s) -> FACTORY RESET TRIGGERED! ");
                Serial.println("  Erasing all NVS settings and restarting to AP mode... ");
                Serial.println("=======================================================\n");

                nvs_flash_erase();
                nvs_flash_init();

                AppConfig blankCfg;
                ConfigManager::save(blankCfg);
                delay(600);
                ESP.restart();
            }
        }
    } else if (s_btnWasPressed) {
        uint32_t pressDuration = millis() - s_btnPressStartTime;
        s_btnWasPressed = false;

        if (!s_factoryResetTriggered && pressDuration >= 50) {
            Serial.printf("[Button] SHORT PRESS (%u ms) -> Reconnecting BLE...\n", (unsigned int)pressDuration);
            g_bleClient.reconnect();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==========================================");
    Serial.println("  Universal BMS Smart Monitor (ESP32-C6)  ");
    Serial.println("==========================================");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Load config from NVS
    g_config = ConfigManager::load();
    Serial.printf("[Config] Loaded -> Target MAC: '%s', Type: %u, Cells: %u\n",
                  g_config.bms_mac.c_str(), g_config.bms_type, g_config.cell_count);

    // Start BLE Client
    g_bleClient.init();
    g_bleClient.setTargetConfig(g_config);

    // Setup Wi-Fi & WebServer
    setupWifi();
    setupHttpRoutes();

    Serial.println("[System] Initialization complete. BOOT button: Short click = Reconnect BLE, Long click (>3s) = Factory Reset.");
}

void loop() {
    handleButton();
    if (g_isApMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
    g_bleClient.loop();
    delay(2);
}
