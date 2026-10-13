#include "app_config.h"

#include <ArduinoJson.h>

// PlatformIO 嵌入文件生成的符号（board_build.embed_files = config.json）
extern const uint8_t configJsonStart[] asm("_binary_config_json_start");
extern const uint8_t configJsonEnd[] asm("_binary_config_json_end");

bool loadAppConfig(AppConfig& cfg)
{
    size_t len = (size_t)(configJsonEnd - configJsonStart);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, configJsonStart, len);
    if (err) {
        Serial.printf("[配置] config.json 解析失败: %s\n", err.c_str());
        return false;
    }

    cfg.wifiSsid       = doc["wifi_ssid"] | "";
    cfg.wifiPassword   = doc["wifi_password"] | "";
    cfg.ntpServer      = doc["ntp_server"] | "ntp.aliyun.com";
    cfg.timezoneOffsetHours = doc["timezone_offset_hours"] | 8;
    cfg.latitude       = doc["latitude"] | 0.0f;
    cfg.longitude      = doc["longitude"] | 0.0f;
    cfg.sleepStartHour = doc["sleep_start_hour"] | 0;
    cfg.sleepEndHour   = doc["sleep_end_hour"] | 7;
    cfg.refreshSeconds = doc["refresh_seconds"] | 60;
    cfg.checkIntervalMinutes  = doc["check_interval_minutes"] | 15;
    cfg.statsDays             = doc["stats_days"] | 3;
    cfg.fullRefreshMinutes    = doc["full_refresh_minutes"] | 60;
    cfg.weatherRefreshMinutes = doc["weather_refresh_minutes"] | 10;

    cfg.checkCount = 0;
    for (JsonObject item : doc["checks"].as<JsonArray>()) {
        if (cfg.checkCount >= 2) break;
        if (!item["url"].is<const char*>()) continue;
        cfg.checks[cfg.checkCount].name = item["name"] | "";
        cfg.checks[cfg.checkCount].url  = item["url"] | "";
        cfg.checkCount++;
    }

    Serial.printf("[配置] WiFi=%s 时区=%+d 坐标=%.4f,%.4f 检测目标=%u 个\n",
                  cfg.wifiSsid.c_str(), (int)cfg.timezoneOffsetHours,
                  cfg.latitude, cfg.longitude, cfg.checkCount);
    return cfg.wifiSsid.length() > 0;
}
