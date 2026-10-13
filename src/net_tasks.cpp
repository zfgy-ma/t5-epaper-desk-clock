#include "net_tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// 可用性统计：存放在 RTC 内存，跨深度睡眠保留（断电后清零）
RTC_DATA_ATTR static Availability g_availability[2];

bool wifiEnsure(const AppConfig& cfg, uint32_t timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.printf("[网络] 连接 WiFi：%s\n", cfg.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[网络] 已连接，IP=%s，信号 %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }
    Serial.println("[网络] 连接失败");
    return false;
}

void timeStartSync(const AppConfig& cfg)
{
    configTime(cfg.timezoneOffsetHours * 3600L, 0, cfg.ntpServer.c_str());
    Serial.printf("[时间] 已启动 NTP 校时（%s，时区 %+d）\n",
                  cfg.ntpServer.c_str(), (int)cfg.timezoneOffsetHours);
}

bool timeIsValid()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) >= 2020;
}

bool weatherFetch(const AppConfig& cfg, WeatherInfo& out)
{
    String url = "https://api.open-meteo.com/v1/forecast?latitude="
                 + String(cfg.latitude, 4) + "&longitude=" + String(cfg.longitude, 4)
                 + "&current=temperature_2m,weather_code";

    WiFiClientSecure client;
    client.setInsecure();   // 公开只读接口，省去证书链配置

    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("[天气] 请求失败，HTTP %d\n", status);
        http.end();
        return false;
    }

    // 只解析需要的字段，节省内存
    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"]   = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("[天气] 解析失败：%s\n", err.c_str());
        return false;
    }

    out.temperature = doc["current"]["temperature_2m"] | 0.0f;
    out.code        = doc["current"]["weather_code"] | 0;
    out.valid       = true;
    out.updatedMs   = millis();
    Serial.printf("[天气] %d°C，天气码 %u\n",
                  (int)lroundf(out.temperature), (unsigned)out.code);
    return true;
}

bool urlCheck(const String& url, uint32_t& latencyMs)
{
    latencyMs = 0;
    if (WiFi.status() != WL_CONNECTED) return false;

    bool isHttps = url.startsWith("https");

    HTTPClient http;
    http.setTimeout(5000);
    http.setConnectTimeout(3000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setInsecure();
        if (!http.begin(secureClient, url)) return false;
    } else {
        if (!http.begin(plainClient, url)) return false;
    }

    uint32_t t0 = millis();
    int status = http.GET();
    latencyMs = millis() - t0;
    http.end();

    // 收到 HTTP 响应（含 4xx）视为站点可达；5xx 或无法连接视为异常
    bool ok = (status > 0 && status < 500);
    return ok;
}

void availabilityRecord(uint8_t index, bool ok)
{
    if (index >= 2) return;

    Availability& a = g_availability[index];
    a.windowBits = (a.windowBits << 1) | (ok ? 1ULL : 0ULL);
    if (a.windowCount < 64) a.windowCount++;
    a.total++;
    if (ok) a.ok++;
}

uint8_t availabilityPercent(uint8_t index)
{
    if (index >= 2) return 255;

    const Availability& a = g_availability[index];
    if (a.windowCount == 0) return 255;   // 暂无数据

    uint8_t good = (uint8_t)__builtin_popcountll(a.windowBits);
    return (uint8_t)lroundf(good * 100.0f / a.windowCount);
}
