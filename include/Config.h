#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>

enum BmsTypeConfig {
    BMS_TYPE_AUTO   = 0,
    BMS_TYPE_JK     = 1,
    BMS_TYPE_JBD    = 2,
    BMS_TYPE_DALY   = 3,
    BMS_TYPE_ANT    = 4,
    BMS_TYPE_REDODO = 5
};

struct AppConfig {
    String wifi_ssid  = "";
    String wifi_pass  = "";
    String bms_mac    = "";
    String bms_name   = "";
    uint8_t bms_type  = BMS_TYPE_AUTO; // 0=Auto, 1=JK, 2=JBD, 3=Daly, 4=Ant, 5=Redodo
    String bms_pin    = "1234";
    uint8_t cell_count = 8;            // 4, 8, 16, 24
};

struct TelegramConfig {
    bool enabled           = false;    // Default: disabled (optional component)
    String bot_token       = "";       // Telegram Bot Token (e.g. 123456789:ABCdefGhI...)
    String chat_ids        = "";       // Authorized User/Chat IDs (comma-separated: 12345678, 87654321)
    bool alerts_enabled    = true;     // Send proactive alerts on critical conditions
    uint16_t alert_delta_mv = 100;     // Cell imbalance threshold (mV)
    int8_t alert_temp_c    = 55;       // Overheat threshold (°C)
    uint8_t alert_low_soc  = 10;       // Critical discharge SOC threshold (%)
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

    // Factory reset: clears only Wi-Fi and BMS settings in 'bms_mon' namespace.
    // Preserves Telegram configuration in 'bms_tg' namespace intact!
    static void clearAppConfig() {
        Preferences prefs;
        prefs.begin("bms_mon", false);
        prefs.clear();
        prefs.end();
    }

    // Telegram configuration management in dedicated 'bms_tg' namespace
    static TelegramConfig loadTelegram() {
        TelegramConfig tg;
        Preferences prefs;
        if (prefs.begin("bms_tg", true)) {
            tg.enabled          = prefs.getBool("en", false);
            tg.bot_token        = prefs.getString("token", "");
            tg.chat_ids         = prefs.getString("chats", "");
            tg.alerts_enabled   = prefs.getBool("alerts", true);
            tg.alert_delta_mv   = prefs.getUShort("dmv", 100);
            tg.alert_temp_c     = (int8_t)prefs.getChar("tc", 55);
            tg.alert_low_soc    = prefs.getUChar("lsoc", 10);
            prefs.end();
        }
        return tg;
    }

    static void saveTelegram(const TelegramConfig& tg) {
        Preferences prefs;
        prefs.begin("bms_tg", false);
        prefs.putBool("en", tg.enabled);
        prefs.putString("token", tg.bot_token);
        prefs.putString("chats", tg.chat_ids);
        prefs.putBool("alerts", tg.alerts_enabled);
        prefs.putUShort("dmv", tg.alert_delta_mv);
        prefs.putChar("tc", tg.alert_temp_c);
        prefs.putUChar("lsoc", tg.alert_low_soc);
        prefs.end();
    }

    // Dedicated button / action to explicitly clear Telegram credentials
    static void clearTelegram() {
        Preferences prefs;
        prefs.begin("bms_tg", false);
        prefs.clear();
        prefs.end();
    }
};
