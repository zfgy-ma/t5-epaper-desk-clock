#include "net_tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// 可用性统计：保存在 NVS 闪存，掉电、重启都不丢失
static const char *NVS_NAMESPACE = "clock";
static Availability g_availability[2];
static uint16_t g_windowSamples = 288;   // 默认窗口：3 天 × 96 次/天

// 把某个站点的统计写入 NVS
static void availabilitySave(uint8_t index)
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    char key[8];
    snprintf(key, sizeof(key), "avail%d", index);
    prefs.putBytes(key, &g_availability[index], sizeof(Availability));
    prefs.end();
}

void availabilityInit(uint16_t windowSamples)
{
    if (windowSamples == 0 || windowSamples > AVAIL_MAX_SAMPLES) {
        windowSamples = AVAIL_MAX_SAMPLES;
    }
    g_windowSamples = windowSamples;

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);   // 可写打开：首次运行自动创建命名空间
    for (uint8_t i = 0; i < 2; i++) {
        char key[8];
        snprintf(key, sizeof(key), "avail%d", i);
        size_t len = prefs.getBytesLength(key);
        if (len == sizeof(Availability)) {
            prefs.getBytes(key, &g_availability[i], sizeof(Availability));
        }
    }
    prefs.end();

    for (uint8_t i = 0; i < 2; i++) {
        uint8_t pct = availabilityPercent(i);
        char pctText[8];
        if (pct == 255) {
            snprintf(pctText, sizeof(pctText), "--");
        } else {
            snprintf(pctText, sizeof(pctText), "%u%%", (unsigned)pct);
        }
        Serial.printf("[统计] 站点 %u 已恢复：样本 %u 个，窗口 %u，可用率 %s\n",
                      (unsigned)i, g_availability[i].count, g_windowSamples,
                      pctText);
    }
}

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

    // 读取完整响应体（便于调试与字段校验）
    String payload = http.getString();
    http.end();
    Serial.printf("[天气] 响应: %s\n", payload.c_str());

    // 只解析需要的字段，节省内存
    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"]   = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (err) {
        Serial.printf("[天气] 解析失败：%s\n", err.c_str());
        return false;
    }

    if (doc["current"]["temperature_2m"].isNull()) {
        Serial.println("[天气] 响应缺少 temperature_2m 字段");
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

    // 环形写入一个位样本：1=可用，0=不可用
    uint16_t pos = a.next;
    if (ok) {
        a.bits[pos / 8] |= (uint8_t)(1 << (pos % 8));
    } else {
        a.bits[pos / 8] &= (uint8_t)~(1 << (pos % 8));
    }
    a.next = (uint16_t)((pos + 1) % AVAIL_MAX_SAMPLES);
    if (a.count < AVAIL_MAX_SAMPLES) a.count++;

    a.total++;
    if (ok) a.ok++;

    availabilitySave(index);   // 立即持久化，掉电不丢
}

uint8_t availabilityPercent(uint8_t index)
{
    if (index >= 2) return 255;

    const Availability& a = g_availability[index];
    uint16_t n = a.count < g_windowSamples ? a.count : g_windowSamples;
    if (n == 0) return 255;   // 暂无数据

    // 统计最近 n 个样本（环形缓冲，最新样本在 next-1 位置）
    uint16_t good = 0;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t pos = (uint16_t)((a.next + AVAIL_MAX_SAMPLES - 1 - i) % AVAIL_MAX_SAMPLES);
        if (a.bits[pos / 8] & (1 << (pos % 8))) good++;
    }
    return (uint8_t)((uint32_t)good * 100 / n);
}
