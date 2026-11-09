#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "BmsBleClient.h"

class TelegramBotClient {
public:
    TelegramBotClient() {}

    void init(BmsBleClient* bleClient) {
        m_bleClient = bleClient;
        reloadConfig();

        // Create dedicated FreeRTOS worker task with lower priority (1)
        // so BLE Central Task (priority 2) always has precedence on RF radio
        xTaskCreate(
            [](void* param) {
                TelegramBotClient* self = static_cast<TelegramBotClient*>(param);
                self->workerLoop();
            },
            "tg_worker",
            8192,
            this,
            1,
            &m_taskHandle
        );
        Serial.println("[Telegram] Background worker task initialized (Priority 1, 8KB stack)");
    }

    void reloadConfig() {
        m_config = ConfigManager::loadTelegram();
        Serial.printf("[Telegram] Config loaded -> Enabled: %s, Token: %s, Authorized IDs: '%s', Alerts: %s (ΔV>%umV, T>%d°C, SOC<%u%%)\n",
                      m_config.enabled ? "YES" : "NO",
                      m_config.bot_token.length() > 6 ? (m_config.bot_token.substring(0, 6) + "...").c_str() : "EMPTY",
                      m_config.chat_ids.c_str(),
                      m_config.alerts_enabled ? "YES" : "NO",
                      m_config.alert_delta_mv,
                      m_config.alert_temp_c,
                      m_config.alert_low_soc);
    }

    bool isEnabled() const { return m_config.enabled && m_config.bot_token.length() > 0; }
    const TelegramConfig& getConfig() const { return m_config; }

    // Test send message from Web UI (supports testing credentials directly from form)
    bool sendTestNotification(String& outError, const String& overrideToken = "", const String& overrideChats = "") {
        String token = overrideToken.length() > 0 ? overrideToken : m_config.bot_token;
        String chats = overrideChats.length() > 0 ? overrideChats : m_config.chat_ids;
        token.trim();
        chats.trim();

        if (token.length() == 0) {
            outError = "Токен Telegram-бота порожній! Введіть токен від @BotFather.";
            return false;
        }
        if (chats.length() == 0) {
            outError = "Не вказано Telegram User ID!";
            return false;
        }
        if (WiFi.status() != WL_CONNECTED) {
            outError = "ESP32 не підключена до Wi-Fi або відсутній вихід в інтернет!";
            return false;
        }

        String msg = "🔔 <b>Тестове сповіщення від Universal BMS Monitor!</b>\n\n"
                     "✅ ESP32-C6 успішно підключено до Telegram Bot API.\n"
                     "Готовий до моніторингу та діагностики BMS.";

        bool anySuccess = false;
        String lastErr = "";
        int start = 0;
        while (start < chats.length()) {
            int sep = chats.indexOf(',', start);
            if (sep == -1) sep = chats.indexOf(' ', start);
            if (sep == -1) sep = chats.indexOf(';', start);
            if (sep == -1) sep = chats.length();

            String idStr = chats.substring(start, sep);
            idStr.trim();
            if (idStr.length() > 0) {
                int64_t chatId = strtoll(idStr.c_str(), nullptr, 10);
                if (chatId != 0) {
                    String tgErr;
                    if (sendMessage(chatId, msg, "", "HTML", token, &tgErr)) {
                        anySuccess = true;
                    } else {
                        lastErr = tgErr;
                    }
                }
            }
            start = sep + 1;
        }

        if (!anySuccess) {
            outError = lastErr.length() > 0 ? lastErr : "Не вдалося доставити повідомлення. Перевірте токен та User ID.";
            return false;
        }
        return true;
    }

private:
    BmsBleClient* m_bleClient = nullptr;
    TelegramConfig m_config;
    TaskHandle_t m_taskHandle = nullptr;
    int32_t m_lastUpdateId = 0;

    // Alert monitoring state
    bool m_alertDeltaActive = false;
    uint32_t m_lastDeltaAlertTime = 0;

    bool m_alertTempActive = false;
    uint32_t m_lastTempAlertTime = 0;

    bool m_alertLowSocActive = false;
    uint32_t m_lastLowSocAlertTime = 0;

    bool m_alertBleDisconnectActive = false;
    uint32_t m_bleDisconnectStartTime = 0;
    bool m_wasBleConnected = false;

    // Cooldown interval for recurring critical alerts (15 minutes)
    static const uint32_t ALERT_COOLDOWN_MS = 15 * 60 * 1000;

    void workerLoop() {
        while (true) {
            if (!isEnabled() || WiFi.status() != WL_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }

            // 1. Process incoming commands & inline button callbacks
            pollUpdates();

            // 2. Check for critical alerts (Delta V, Temp, SOC, Disconnect)
            if (m_config.alerts_enabled) {
                checkAlerts();
            }

            // Smooth pause between polls: preserves Wi-Fi/BLE RF coexistence
            vTaskDelay(pdMS_TO_TICKS(1800));
        }
    }

    // Check if chat_id is present in comma/space-separated list
    bool isAuthorized(int64_t chatId) {
        if (m_config.chat_ids.length() == 0) return false;
        String idStr = String(chatId);
        int start = 0;
        while (start < m_config.chat_ids.length()) {
            int sep = m_config.chat_ids.indexOf(',', start);
            if (sep == -1) sep = m_config.chat_ids.indexOf(' ', start);
            if (sep == -1) sep = m_config.chat_ids.indexOf(';', start);
            if (sep == -1) sep = m_config.chat_ids.length();

            String token = m_config.chat_ids.substring(start, sep);
            token.trim();
            if (token == idStr) return true;
            start = sep + 1;
        }
        return false;
    }

    // Broadcast message to all authorized IDs
    bool broadcast(const String& message, const String& replyMarkup = "") {
        if (m_config.chat_ids.length() == 0) return false;
        bool anySuccess = false;
        int start = 0;
        while (start < m_config.chat_ids.length()) {
            int sep = m_config.chat_ids.indexOf(',', start);
            if (sep == -1) sep = m_config.chat_ids.indexOf(' ', start);
            if (sep == -1) sep = m_config.chat_ids.indexOf(';', start);
            if (sep == -1) sep = m_config.chat_ids.length();

            String token = m_config.chat_ids.substring(start, sep);
            token.trim();
            if (token.length() > 0) {
                int64_t chatId = strtoll(token.c_str(), nullptr, 10);
                if (chatId != 0) {
                    if (sendMessage(chatId, message, replyMarkup)) {
                        anySuccess = true;
                    }
                }
            }
            start = sep + 1;
        }
        return anySuccess;
    }

    // Low-level HTTPS API call to Telegram
    String sendApiRequest(const String& method, const String& jsonPayload, const String& overrideToken = "") {
        String token = overrideToken.length() > 0 ? overrideToken : m_config.bot_token;
        if (WiFi.status() != WL_CONNECTED || token.length() == 0) {
            return "";
        }

        WiFiClientSecure client;
        client.setInsecure(); // No heavy CA certificates in RAM
        client.setTimeout(5000);

        HTTPClient http;
        String url = "https://api.telegram.org/bot" + token + "/" + method;
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");

        int httpCode = http.POST(jsonPayload);
        String response = "";
        if (httpCode > 0) {
            response = http.getString();
        } else {
            Serial.printf("[Telegram] HTTP POST '%s' failed, err: %s\n", method.c_str(), http.errorToString(httpCode).c_str());
        }
        http.end(); // Closes socket and frees mbedTLS buffers immediately
        return response;
    }

    bool sendMessage(int64_t chatId, const String& text, const String& replyMarkupJson = "", const String& parseMode = "HTML", const String& overrideToken = "", String* outApiError = nullptr) {
        JsonDocument doc;
        doc["chat_id"] = chatId;
        doc["text"] = text;
        doc["parse_mode"] = parseMode;
        doc["disable_web_page_preview"] = true;

        if (replyMarkupJson.length() > 0) {
            JsonDocument markupDoc;
            DeserializationError err = deserializeJson(markupDoc, replyMarkupJson);
            if (!err) {
                doc["reply_markup"] = markupDoc;
            }
        }

        String payload;
        serializeJson(doc, payload);
        String resp = sendApiRequest("sendMessage", payload, overrideToken);

        JsonDocument resDoc;
        DeserializationError dErr = deserializeJson(resDoc, resp);
        if (dErr) {
            if (outApiError) *outApiError = "Не вдалося розібрати відповідь від Telegram API.";
            return false;
        }

        bool ok = resDoc["ok"].as<bool>();
        if (!ok && outApiError) {
            String desc = resDoc["description"].as<String>();
            if (desc.indexOf("chat not found") != -1) {
                *outApiError = "Чат не знайдено! Відкрийте в Telegram вашого бота та натисніть кнопку START (Розпочати), після чого натисніть 'Тест сповіщення' знову.";
            } else if (desc.indexOf("Unauthorized") != -1) {
                *outApiError = "Невірний токен бота (Unauthorized)! Перевірте правильність токена від @BotFather.";
            } else if (desc.length() > 0) {
                *outApiError = desc;
            } else {
                *outApiError = "Помилка доставки повідомлення через Telegram API.";
            }
        }
        return ok;
    }

    bool editMessageText(int64_t chatId, int32_t messageId, const String& text, const String& replyMarkupJson = "", const String& parseMode = "HTML") {
        JsonDocument doc;
        doc["chat_id"] = chatId;
        doc["message_id"] = messageId;
        doc["text"] = text;
        doc["parse_mode"] = parseMode;
        doc["disable_web_page_preview"] = true;

        if (replyMarkupJson.length() > 0) {
            JsonDocument markupDoc;
            DeserializationError err = deserializeJson(markupDoc, replyMarkupJson);
            if (!err) {
                doc["reply_markup"] = markupDoc;
            }
        }

        String payload;
        serializeJson(doc, payload);
        String resp = sendApiRequest("editMessageText", payload);

        JsonDocument resDoc;
        deserializeJson(resDoc, resp);
        return resDoc["ok"].as<bool>();
    }

    bool answerCallbackQuery(const String& callbackQueryId, const String& text = "", bool showAlert = false) {
        JsonDocument doc;
        doc["callback_query_id"] = callbackQueryId;
        if (text.length() > 0) doc["text"] = text;
        doc["show_alert"] = showAlert;

        String payload;
        serializeJson(doc, payload);
        String resp = sendApiRequest("answerCallbackQuery", payload);
        return true;
    }

    // Helper string formatters
    static String getBmsTypeName(uint8_t type) {
        switch (type) {
            case BMS_TYPE_JK: return "JK-BMS";
            case BMS_TYPE_JBD: return "JBD-BMS";
            case BMS_TYPE_DALY: return "Daly-BMS";
            case BMS_TYPE_ANT: return "Ant-BMS";
            case BMS_TYPE_REDODO: return "Redodo/LiTime";
            default: return "Auto";
        }
    }

    String buildStatusMessage() {
        if (!m_bleClient) return "⚠️ Клієнт BMS недоступний.";
        BmsTelemetry telem = m_bleClient->getTelemetry();
        const AppConfig& cfg = m_bleClient->getConfig();

        String bmsName = telem.device_name.length() > 0 ? telem.device_name : (cfg.bms_name.length() > 0 ? cfg.bms_name : "BMS Battery");
        String bmsType = (telem.bms_type.length() > 0 && telem.bms_type != "None") ? telem.bms_type : getBmsTypeName(cfg.bms_type);

        String msg = "📊 <b>Стан батареї — " + bmsName + "</b>\n";
        msg += "• Тип: <code>" + bmsType + "</code> | MAC: <code>" + (telem.mac_address.length() > 0 ? telem.mac_address : cfg.bms_mac) + "</code>\n";
        msg += "• Зв'язок BLE: " + String(telem.connected ? "🟢 Підключено" : "🔴 Відключено") + "\n\n";

        if (!telem.connected) {
            msg += "⚠️ <i>Очікування підключення Bluetooth до BMS...</i>\n";
            return msg;
        }

        // Key metrics
        msg += "⚡ <b>Напруга АКБ:</b> <code>" + String(telem.total_voltage, 2) + " V</code>\n";
        msg += "🔌 <b>Струм:</b> <code>" + String(telem.current >= 0 ? "+" : "") + String(telem.current, 2) + " A</code>\n";
        msg += "💡 <b>Потужність:</b> <code>" + String(telem.power >= 0 ? "+" : "") + String(telem.power, 1) + " W</code>\n";
        msg += "🔋 <b>Заряд (SOC):</b> <code>" + String(telem.soc) + "%</code> (" + String(telem.capacity_remain, 1) + " / " + String(telem.capacity_nominal, 1) + " Ah)\n";
        msg += "🔄 <b>Циклів:</b> <code>" + String(telem.cycle_count) + "</code>\n\n";

        // Imbalance & Cells summary
        uint8_t count = cfg.cell_count > 0 ? cfg.cell_count : (telem.cell_count > 0 ? telem.cell_count : 8);
        float deltaMv = telem.delta_cell_v * 1000.0f;
        String deltaIcon = (deltaMv >= 100.0f) ? "🚨 " : (deltaMv >= 50.0f ? "⚠️ " : "✅ ");
        msg += "📐 <b>Дисбаланс ΔV:</b> " + deltaIcon + "<code>" + String(deltaMv, 0) + " mV</code>\n";
        msg += "• Min: C" + String(telem.min_cell_idx + 1) + " (<code>" + String(telem.min_cell_v, 3) + " V</code>) 🔻\n";
        msg += "• Max: C" + String(telem.max_cell_idx + 1) + " (<code>" + String(telem.max_cell_v, 3) + " V</code>) 🔺\n\n";

        // Temperatures
        msg += "🌡️ <b>Температури:</b>\n";
        msg += "• MOS: <code>" + String(telem.temp_mos, 1) + "°C</code> | Т1: <code>" + String(telem.temp_sensor1, 1) + "°C</code> | Т2: <code>" + String(telem.temp_sensor2, 1) + "°C</code>\n\n";

        // Switches & Balancer
        msg += "⚙️ <b>Стан ключів:</b>\n";
        msg += "• Заряд (CHG): " + String(telem.switch_charging ? "🟢 УВІМК" : "🔴 ВИМК") + "\n";
        msg += "• Розряд (DSG): " + String(telem.switch_discharging ? "🟢 УВІМК" : "🔴 ВИМК") + "\n";
        msg += "• Балансир: " + String(telem.switch_balancer ? "🟢 УВІМК" : "⚪ ВИМК");
        if (telem.balancing_active) {
            msg += " (⚡ Активний: " + String(telem.balancing_current, 2) + "A)";
        }
        msg += "\n";

        if (telem.errors_str.length() > 0) {
            msg += "\n🚨 <b>Захисти / Помилки:</b> <code>" + telem.errors_str + "</code>\n";
        }

        return msg;
    }

    String buildStatusKeyboard() {
        return "{\"inline_keyboard\":["
               "[{\"text\":\"🔄 Оновити\",\"callback_data\":\"status_refresh\"},"
               "{\"text\":\"🔋 Комірки\",\"callback_data\":\"show_cells\"}],"
               "[{\"text\":\"⚡ Керування ключами\",\"callback_data\":\"show_control\"}]"
               "]}";
    }

    String buildCellsMessage() {
        if (!m_bleClient) return "⚠️ Клієнт BMS недоступний.";
        BmsTelemetry telem = m_bleClient->getTelemetry();
        const AppConfig& cfg = m_bleClient->getConfig();
        uint8_t count = cfg.cell_count > 0 ? cfg.cell_count : (telem.cell_count > 0 ? telem.cell_count : 8);

        String msg = "🔋 <b>Напруга комірок (" + String(count) + "S):</b>\n<pre>\n";
        for (uint8_t i = 0; i < count; i++) {
            float v = (i < 32) ? telem.cell_voltages[i] : 0.0f;
            char buf[32];
            snprintf(buf, sizeof(buf), "C%02u: %.3f V", i + 1, v);
            msg += buf;
            if (i == telem.min_cell_idx) msg += " 🔻";
            else if (i == telem.max_cell_idx) msg += " 🔺";
            else if (telem.balancing_active && (telem.balancing_mask & (1UL << i))) msg += " ⚡";
            msg += "\n";
        }
        msg += "</pre>\n";

        float deltaMv = telem.delta_cell_v * 1000.0f;
        String deltaIcon = (deltaMv >= 100.0f) ? "🚨 " : (deltaMv >= 50.0f ? "⚠️ " : "✅ ");
        msg += "🔻 <b>Мінімум:</b> C" + String(telem.min_cell_idx + 1) + " (<code>" + String(telem.min_cell_v, 3) + " V</code>)\n";
        msg += "🔺 <b>Максимум:</b> C" + String(telem.max_cell_idx + 1) + " (<code>" + String(telem.max_cell_v, 3) + " V</code>)\n";
        msg += "⚡ <b>Дисбаланс ΔV:</b> " + deltaIcon + "<code>" + String(deltaMv, 0) + " mV</code>\n";
        return msg;
    }

    String buildCellsKeyboard() {
        return "{\"inline_keyboard\":["
               "[{\"text\":\"🔄 Оновити комірки\",\"callback_data\":\"cells_refresh\"},"
               "{\"text\":\"📊 Статус АКБ\",\"callback_data\":\"status_refresh\"}],"
               "[{\"text\":\"⚡ Керування ключами\",\"callback_data\":\"show_control\"}]"
               "]}";
    }

    String buildControlMessage() {
        if (!m_bleClient) return "⚠️ Клієнт BMS недоступний.";
        BmsTelemetry telem = m_bleClient->getTelemetry();

        String msg = "⚡ <b>Керування силовими ключами BMS</b>\n\n";
        msg += "• Зарядний ключ (CHG MOS): " + String(telem.switch_charging ? "🟢 УВІМКНЕНО" : "🔴 ВИМКНЕНО") + "\n";
        msg += "• Розрядний ключ (DSG MOS): " + String(telem.switch_discharging ? "🟢 УВІМКНЕНО" : "🔴 ВИМКНЕНО") + "\n";
        msg += "• Функція балансира: " + String(telem.switch_balancer ? "🟢 УВІМКНЕНО" : "⚪ ВИМКНЕНО") + "\n\n";
        msg += "⚠️ <i>Оберіть дію нижче. Для безпеки кожна дія вимагає обов'язкового підтвердження!</i>";
        return msg;
    }

    String buildControlKeyboard() {
        if (!m_bleClient) return "{\"inline_keyboard\":[]}";
        BmsTelemetry telem = m_bleClient->getTelemetry();

        String chgBtn = telem.switch_charging ?
            "{\"text\":\"🔴 Вимкнути заряд\",\"callback_data\":\"req_chg_off\"}" :
            "{\"text\":\"🟢 Увімкнути заряд\",\"callback_data\":\"req_chg_on\"}";

        String dsgBtn = telem.switch_discharging ?
            "{\"text\":\"🔴 Вимкнути розряд\",\"callback_data\":\"req_dis_off\"}" :
            "{\"text\":\"🟢 Увімкнути розряд\",\"callback_data\":\"req_dis_on\"}";

        String balBtn = telem.switch_balancer ?
            "{\"text\":\"⚪ Вимкнути балансир\",\"callback_data\":\"req_bal_off\"}" :
            "{\"text\":\"🟢 Увімкнути балансир\",\"callback_data\":\"req_bal_on\"}";

        return "{\"inline_keyboard\":["
               "[" + chgBtn + "],"
               "[" + dsgBtn + "],"
               "[" + balBtn + "],"
               "[{\"text\":\"🔄 Оновити\",\"callback_data\":\"control_refresh\"},"
               "{\"text\":\"📊 Назад до статусу\",\"callback_data\":\"status_refresh\"}]"
               "]}";
    }

    String buildConfirmKeyboard(const String& confirmCallback) {
        return "{\"inline_keyboard\":["
               "[{\"text\":\"✅ ТАК, ПІДТВЕРДЖУЮ\",\"callback_data\":\"" + confirmCallback + "\"}],"
               "[{\"text\":\"❌ СКАСУВАТИ\",\"callback_data\":\"show_control\"}]"
               "]}";
    }

    String buildInfoMessage() {
        String msg = "ℹ️ <b>Системна інформація ESP32-C6:</b>\n\n";
        msg += "• Прошивка: <b>Universal BMS Monitor v1.3.0 (Telegram)</b>\n";
        msg += "• Вільний RAM (Heap): <code>" + String(ESP.getFreeHeap() / 1024) + " KB</code>\n";
        msg += "• Wi-Fi RSSI: <code>" + String(WiFi.RSSI()) + " dBm</code>\n";
        msg += "• IP адреса: <code>" + WiFi.localIP().toString() + "</code>\n";
        msg += "• Час роботи (Uptime): <code>" + String(millis() / 60000) + " хв</code>\n";
        if (m_bleClient) {
            BmsTelemetry telem = m_bleClient->getTelemetry();
            msg += "• BLE статус: " + String(telem.connected ? "🟢 З'єднано з BMS" : "🔴 Пошук BMS...") + "\n";
        }
        return msg;
    }

    // Process polling updates from Telegram
    void pollUpdates() {
        String urlParams = "getUpdates?offset=" + String(m_lastUpdateId + 1) + "&limit=3&timeout=0";
        String resp = sendApiRequest(urlParams, "{}");
        if (resp.length() == 0) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (err || !doc["ok"].as<bool>()) return;

        JsonArray results = doc["result"].as<JsonArray>();
        for (JsonObject update : results) {
            int32_t updateId = update["update_id"].as<int32_t>();
            if (updateId >= m_lastUpdateId) {
                m_lastUpdateId = updateId;
            }

            // Normal text message
            if (update["message"].is<JsonObject>()) {
                JsonObject msg = update["message"].as<JsonObject>();
                int64_t chatId = msg["chat"]["id"].as<int64_t>();
                String text = msg["text"].as<String>();
                handleIncomingMessage(chatId, text);
            }
            // Inline button callback query
            else if (update["callback_query"].is<JsonObject>()) {
                JsonObject cb = update["callback_query"].as<JsonObject>();
                String queryId = cb["id"].as<String>();
                String data = cb["data"].as<String>();
                int64_t chatId = cb["message"]["chat"]["id"].as<int64_t>();
                int32_t msgId = cb["message"]["message_id"].as<int32_t>();
                handleCallbackQuery(queryId, chatId, msgId, data);
            }
        }
    }

    void handleIncomingMessage(int64_t chatId, String text) {
        text.trim();
        Serial.printf("[Telegram] Message from Chat ID %lld: '%s'\n", (long long)chatId, text.c_str());

        // Authorization check
        if (!isAuthorized(chatId)) {
            Serial.printf("[Telegram] Unauthorized access attempt from Chat ID: %lld\n", (long long)chatId);
            String denied = "⛔ <b>Доступ заборонено!</b>\n\n"
                            "Ваш User ID: <code>" + String(chatId) + "</code>\n"
                            "Цей ID не внесений до списку дозволених адміністраторів у налаштуваннях ESP32-C6.\n"
                            "Для надання доступу додайте цей ID через Web Dashboard: http://bms-monitor.local/setup";
            sendMessage(chatId, denied);
            return;
        }

        // Handle slash commands
        if (text == "/start" || text == "/help") {
            String welcome = "👋 <b>Вітаю в Universal BMS Monitor!</b>\n\n"
                             "Пристрій на базі <b>ESP32-C6</b> здійснює моніторинг та віддалену діагностику вашої Smart BMS через Bluetooth LE.\n\n"
                             "<b>Доступні команди:</b>\n"
                             "📊 /status — Поточний стан АКБ, напруга, струм, потужність, SOC, температури\n"
                             "🔋 /cells — Детальна таблиця напруг по кожній комірці та точний дисбаланс ΔV\n"
                             "⚡ /control — Безпечне керування ключами заряду, розряду та балансиром\n"
                             "ℹ️ /info — Системна інформація ESP32-C6 (Wi-Fi, Heap, Uptime)\n\n"
                             "💡 <i>Ви також можете користуватися зручними інлайн-кнопками під повідомленнями.</i>";
            sendMessage(chatId, welcome, buildStatusKeyboard());
        }
        else if (text == "/status") {
            sendMessage(chatId, buildStatusMessage(), buildStatusKeyboard());
        }
        else if (text == "/cells") {
            sendMessage(chatId, buildCellsMessage(), buildCellsKeyboard());
        }
        else if (text == "/control") {
            sendMessage(chatId, buildControlMessage(), buildControlKeyboard());
        }
        else if (text == "/info") {
            sendMessage(chatId, buildInfoMessage());
        }
        else {
            sendMessage(chatId, "❓ Невідома команда. Натисніть /status, /cells або /control.");
        }
    }

    void handleCallbackQuery(const String& queryId, int64_t chatId, int32_t msgId, const String& data) {
        Serial.printf("[Telegram] CallbackQuery from Chat ID %lld: '%s'\n", (long long)chatId, data.c_str());

        if (!isAuthorized(chatId)) {
            answerCallbackQuery(queryId, "⛔ Доступ заборонено!", true);
            return;
        }

        // Navigation & Refresh callbacks
        if (data == "status_refresh") {
            answerCallbackQuery(queryId, "🔄 Оновлення...");
            editMessageText(chatId, msgId, buildStatusMessage(), buildStatusKeyboard());
        }
        else if (data == "show_cells" || data == "cells_refresh") {
            answerCallbackQuery(queryId, "🔋 Оновлення комірок...");
            editMessageText(chatId, msgId, buildCellsMessage(), buildCellsKeyboard());
        }
        else if (data == "show_control" || data == "control_refresh") {
            answerCallbackQuery(queryId, "⚡ Панель керування");
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }

        // Switch Request Confirmation Prompts (Requirement 1)
        else if (data == "req_chg_off") {
            answerCallbackQuery(queryId);
            String prompt = "⚠️ <b>ПІДТВЕРДЖЕННЯ ДІЇ:</b>\n\nВи дійсно бажаєте <b>ВИМКНУТИ ЗАРЯД</b> АКБ (CHG MOS)?\nЗаряджання від інвертора чи ЗП припиниться.";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_chg_off"));
        }
        else if (data == "req_chg_on") {
            answerCallbackQuery(queryId);
            String prompt = "⚡ <b>ПІДТВЕРДЖЕННЯ ДІЇ:</b>\n\nУвімкнути <b>ЗАРЯД</b> АКБ (CHG MOS)?";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_chg_on"));
        }
        else if (data == "req_dis_off") {
            answerCallbackQuery(queryId);
            String prompt = "🚨 <b>УВАГА! КРИТИЧНЕ ПІДТВЕРДЖЕННЯ:</b>\n\n"
                            "Ви дійсно бажаєте <b>ВИМКНУТИ РОЗРЯД</b> АКБ (DSG MOS)?\n"
                            "Це призведе до <b>миттєвого знеструмлення виходу батареї та інвертора / навантаження</b>!";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_dis_off"));
        }
        else if (data == "req_dis_on") {
            answerCallbackQuery(queryId);
            String prompt = "⚡ <b>ПІДТВЕРДЖЕННЯ ДІЇ:</b>\n\nУвімкнути <b>РОЗРЯД</b> АКБ (DSG MOS)?\nНапруга знову подасться на клеми інвертора.";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_dis_on"));
        }
        else if (data == "req_bal_off") {
            answerCallbackQuery(queryId);
            String prompt = "⚠️ <b>ПІДТВЕРДЖЕННЯ ДІЇ:</b>\n\nВимкнути функцію активного <b>БАЛАНСИРА</b>?";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_bal_off"));
        }
        else if (data == "req_bal_on") {
            answerCallbackQuery(queryId);
            String prompt = "⚡ <b>ПІДТВЕРДЖЕННЯ ДІЇ:</b>\n\nУвімкнути функцію активного <b>БАЛАНСИРА</b>?";
            editMessageText(chatId, msgId, prompt, buildConfirmKeyboard("cf_bal_on"));
        }

        // Execute Confirmed Switch Commands
        else if (data == "cf_chg_off") {
            bool ok = m_bleClient ? m_bleClient->setCharging(false) : false;
            answerCallbackQuery(queryId, ok ? "✅ Заряд вимкнено" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
        else if (data == "cf_chg_on") {
            bool ok = m_bleClient ? m_bleClient->setCharging(true) : false;
            answerCallbackQuery(queryId, ok ? "✅ Заряд увімкнено" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
        else if (data == "cf_dis_off") {
            bool ok = m_bleClient ? m_bleClient->setDischarging(false) : false;
            answerCallbackQuery(queryId, ok ? "✅ Розряд ВИМКНЕНО" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
        else if (data == "cf_dis_on") {
            bool ok = m_bleClient ? m_bleClient->setDischarging(true) : false;
            answerCallbackQuery(queryId, ok ? "✅ Розряд УВІМКНЕНО" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
        else if (data == "cf_bal_off") {
            bool ok = m_bleClient ? m_bleClient->setBalancer(false) : false;
            answerCallbackQuery(queryId, ok ? "✅ Балансир вимкнено" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
        else if (data == "cf_bal_on") {
            bool ok = m_bleClient ? m_bleClient->setBalancer(true) : false;
            answerCallbackQuery(queryId, ok ? "✅ Балансир увімкнено" : "❌ Помилка надсилання до BMS", true);
            vTaskDelay(pdMS_TO_TICKS(400));
            editMessageText(chatId, msgId, buildControlMessage(), buildControlKeyboard());
        }
    }

    // Proactive Push Alert Monitoring (Requirement 2)
    void checkAlerts() {
        if (!m_bleClient) return;
        BmsTelemetry telem = m_bleClient->getTelemetry();
        uint32_t now = millis();

        // 1. Connection Tracking
        if (telem.connected) {
            if (m_alertBleDisconnectActive) {
                // Resolved
                m_alertBleDisconnectActive = false;
                broadcast("✅ <b>ВІДНОВЛЕНО ЗВ'ЯЗОК З BMS!</b>\nESP32-C6 знову успішно отримує телеметрію через BLE.");
            }
            m_wasBleConnected = true;
            m_bleDisconnectStartTime = 0;
        } else {
            if (m_wasBleConnected) {
                if (m_bleDisconnectStartTime == 0) m_bleDisconnectStartTime = now;
                // Alert if disconnected for > 45 seconds
                if (now - m_bleDisconnectStartTime > 45000 && !m_alertBleDisconnectActive) {
                    m_alertBleDisconnectActive = true;
                    broadcast("⚠️ <b>УВАГА: ВТРАТА ЗВ'ЯЗКУ З BMS!</b>\nESP32-C6 не може з'єднатися з BMS через Bluetooth вже понад 45 секунд.");
                }
            }
            return; // If not connected, cannot check delta or temps
        }

        // 2. High Cell Imbalance Alert (ΔV > alert_delta_mv)
        float deltaMv = telem.delta_cell_v * 1000.0f;
        if (deltaMv >= m_config.alert_delta_mv) {
            if (!m_alertDeltaActive || (now - m_lastDeltaAlertTime > ALERT_COOLDOWN_MS)) {
                m_alertDeltaActive = true;
                m_lastDeltaAlertTime = now;
                String alert = "🚨 <b>КРИТИЧНИЙ ДИСБАЛАНС КОМІРОК!</b>\n\n"
                               "• Поточний дисбаланс: <b>" + String(deltaMv, 0) + " mV</b> (поріг: " + String(m_config.alert_delta_mv) + " mV)\n"
                               "• Найнижча комірка: C" + String(telem.min_cell_idx + 1) + " (<code>" + String(telem.min_cell_v, 3) + " V</code>) 🔻\n"
                               "• Найвища комірка: C" + String(telem.max_cell_idx + 1) + " (<code>" + String(telem.max_cell_v, 3) + " V</code>) 🔺\n"
                               "• Стан балансира: " + String(telem.balancing_active ? "⚡ Активний" : "⚪ Не активний");
                broadcast(alert, buildCellsKeyboard());
            }
        } else if (m_alertDeltaActive && deltaMv <= (m_config.alert_delta_mv * 0.6f)) {
            // Imbalance normalized (hysteresis at 60% of threshold)
            m_alertDeltaActive = false;
            broadcast("✅ <b>НОРМАЛІЗАЦІЯ ДИСБАЛАНСУ:</b>\nРізниця напруг комірок знизилася до безпечних <b>" + String(deltaMv, 0) + " mV</b>.");
        }

        // 3. Overheat Alert (Temp > alert_temp_c)
        float maxTemp = max(telem.temp_mos, max(telem.temp_sensor1, telem.temp_sensor2));
        if (maxTemp >= m_config.alert_temp_c) {
            if (!m_alertTempActive || (now - m_lastTempAlertTime > ALERT_COOLDOWN_MS)) {
                m_alertTempActive = true;
                m_lastTempAlertTime = now;
                String alert = "🔥 <b>УВАГА: ПЕРЕГРІВ АКБ / СИЛОВИХ MOSFET!</b>\n\n"
                               "• Максимальна температура: <b>" + String(maxTemp, 1) + "°C</b> (поріг: " + String(m_config.alert_temp_c) + "°C)\n"
                               "• Датчик MOSFET: <code>" + String(telem.temp_mos, 1) + "°C</code>\n"
                               "• Сенсор Т1: <code>" + String(telem.temp_sensor1, 1) + "°C</code> | Т2: <code>" + String(telem.temp_sensor2, 1) + "°C</code>";
                broadcast(alert, buildStatusKeyboard());
            }
        } else if (m_alertTempActive && maxTemp <= (m_config.alert_temp_c - 6)) {
            // Overheat normalized
            m_alertTempActive = false;
            broadcast("✅ <b>НОРМАЛІЗАЦІЯ ТЕМПЕРАТУРИ:</b>\nТемпература опустилася до безпечного рівня <b>" + String(maxTemp, 1) + "°C</b>.");
        }

        // 4. Critical Discharge Alert (SOC <= alert_low_soc or min_cell_v < 2.80V)
        bool isLowSoc = (telem.soc <= m_config.alert_low_soc) || (telem.min_cell_v > 0.5f && telem.min_cell_v < 2.80f);
        if (isLowSoc) {
            if (!m_alertLowSocActive || (now - m_lastLowSocAlertTime > ALERT_COOLDOWN_MS)) {
                m_alertLowSocActive = true;
                m_lastLowSocAlertTime = now;
                String alert = "🪫 <b>УВАГА: КРИТИЧНИЙ РОЗРЯД АКБ!</b>\n\n"
                               "• Рівень заряду: <b>" + String(telem.soc) + "%</b> (поріг: " + String(m_config.alert_low_soc) + "%)\n"
                               "• Напруга АКБ: <b>" + String(telem.total_voltage, 2) + " V</b>\n"
                               "• Мінімальна комірка: C" + String(telem.min_cell_idx + 1) + " (<code>" + String(telem.min_cell_v, 3) + " V</code>) 🔻\n"
                               "• Залишок ємності: " + String(telem.capacity_remain, 1) + " Ah";
                broadcast(alert, buildStatusKeyboard());
            }
        } else if (m_alertLowSocActive && telem.soc >= (m_config.alert_low_soc + 5) && telem.min_cell_v >= 3.00f) {
            // SOC normalized
            m_alertLowSocActive = false;
            broadcast("✅ <b>НОРМАЛІЗАЦІЯ ЗАРЯДУ:</b>\nРівень заряду зріс до <b>" + String(telem.soc) + "%</b> (" + String(telem.total_voltage, 2) + "V).");
        }
    }
};
