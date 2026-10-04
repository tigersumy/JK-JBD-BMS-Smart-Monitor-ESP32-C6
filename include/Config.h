#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>

enum BmsTypeConfig {
    BMS_TYPE_AUTO = 0,
    BMS_TYPE_JK   = 1,
    BMS_TYPE_JBD  = 2
};

struct AppConfig {
    String wifi_ssid  = "";
    String wifi_pass  = "";
    String bms_mac    = "";
    String bms_name   = "";
    uint8_t bms_type  = BMS_TYPE_AUTO; // 0=Auto, 1=JK, 2=JBD
    String bms_pin    = "1234";
    uint8_t cell_count = 8;            // 4, 8, 16, 24
};

class ConfigManager {
public:
    static AppConfig load() {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            nvs_flash_init();
        }

        AppConfig cfg;
        Preferences prefs;
        if (prefs.begin("bms_mon", true)) {
            cfg.wifi_ssid   = prefs.getString("ssid", "");
            cfg.wifi_pass   = prefs.getString("pass", "");
            cfg.bms_mac     = prefs.getString("mac", "");
            cfg.bms_name    = prefs.getString("name", "");
            cfg.bms_type    = prefs.getUChar("type", BMS_TYPE_AUTO);
            cfg.bms_pin     = prefs.getString("pin", "1234");
            cfg.cell_count  = prefs.getUChar("cells", 8);
            prefs.end();
        }
        if (cfg.cell_count < 2 || cfg.cell_count > 32) {
            cfg.cell_count = 8;
        }
        return cfg;
    }

    static void save(const AppConfig& cfg) {
        Preferences prefs;
        prefs.begin("bms_mon", false);
        prefs.putString("ssid", cfg.wifi_ssid);
        prefs.putString("pass", cfg.wifi_pass);
        prefs.putString("mac", cfg.bms_mac);
        prefs.putString("name", cfg.bms_name);
        prefs.putUChar("type", cfg.bms_type);
        prefs.putString("pin", cfg.bms_pin);
        prefs.putUChar("cells", cfg.cell_count);
        prefs.end();
    }
};
