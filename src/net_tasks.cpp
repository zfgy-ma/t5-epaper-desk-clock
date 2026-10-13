#include "net_tasks.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// 可用性统计：保存在 NVS 闪存，掉电、重启都不丢失
static const char *NVS_NAMESPACE = "clock";
static Availability g_availability[2];
static uint16_t g_windowSlots   = 288;   // 默认窗口：3 天 × 96 槽/天
static uint16_t g_slotSeconds   = 900;   // 默认每槽 15 分钟
static uint8_t  g_sleepStartHour = 0;    // 夜间休眠开始（小时）
static uint8_t  g_sleepEndHour   = 7;    // 夜间休眠结束（小时）

// 判断某个小时是否处于夜间休眠时段（支持跨零点，如 23 点至 7 点）
static bool hourInSleep(uint8_t hour)
{
    if (g_sleepStartHour == g_sleepEndHour) return false;   // 未配置休眠
    if (g_sleepStartHour < g_sleepEndHour) {
        return hour >= g_sleepStartHour && hour < g_sleepEndHour;
    }
    return hour >= g_sleepStartHour || hour < g_sleepEndHour;
}

// 延迟历史：环形缓冲（NVS 持久化），0xFFFF 表示检测失败
struct LatencyHistory {
    uint16_t count = 0;
    uint16_t next  = 0;
    uint16_t ms[LAT_MAX_SAMPLES] = { 0 };
};
static LatencyHistory g_latency[2];

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

void availabilityInit(uint16_t windowSlots, uint16_t slotSeconds,
                      uint8_t sleepStartHour, uint8_t sleepEndHour)
{
    if (windowSlots == 0 || windowSlots > AVAIL_MAX_SLOTS) {
        windowSlots = AVAIL_MAX_SLOTS;
    }
    if (slotSeconds == 0) slotSeconds = 900;

    g_windowSlots    = windowSlots;
    g_slotSeconds    = slotSeconds;
    g_sleepStartHour = sleepStartHour;
    g_sleepEndHour   = sleepEndHour;

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);   // 可写打开：首次运行自动创建命名空间
    for (uint8_t i = 0; i < 2; i++) {
        char key[8];

        snprintf(key, sizeof(key), "avail%d", i);
        if (prefs.isKey(key)) {
            size_t len = prefs.getBytesLength(key);
            if (len == sizeof(Availability)) {
                prefs.getBytes(key, &g_availability[i], sizeof(Availability));
            }
        }

        snprintf(key, sizeof(key), "lat%u", i);
        if (prefs.isKey(key)) {
            size_t len = prefs.getBytesLength(key);
            if (len == sizeof(LatencyHistory)) {
                prefs.getBytes(key, &g_latency[i], sizeof(LatencyHistory));
            }
        }
    }
    prefs.end();

    Serial.printf("[统计] 时间轴窗口 %u 槽 × %u 秒/槽，夜间 %u 点至 %u 点不采样\n",
                  (unsigned)g_windowSlots, (unsigned)g_slotSeconds,
                  (unsigned)g_sleepStartHour, (unsigned)g_sleepEndHour);
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

bool weatherFetch(const AppConfig& cfg, WeatherInfo& out, ForecastInfo& fc)
{
    String url = "https://api.open-meteo.com/v1/forecast?latitude="
                 + String(cfg.latitude, 4) + "&longitude=" + String(cfg.longitude, 4)
                 + "&current=temperature_2m,weather_code"
                 + "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                 + "&past_days=1&forecast_days=3&timezone=auto";

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
    filter["daily"]["weather_code"]          = true;
    filter["daily"]["temperature_2m_max"]    = true;
    filter["daily"]["temperature_2m_min"]    = true;
    filter["daily"]["time"]                  = true;

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

    // 解析 4 天数据（顺序：昨天、今天、明天、后天）
    JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
    JsonArray tmax  = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    JsonArray tmin  = doc["daily"]["temperature_2m_min"].as<JsonArray>();
    JsonArray times = doc["daily"]["time"].as<JsonArray>();

    fc.valid = false;
    uint8_t n = (uint8_t)codes.size();
    if (n > 4) n = 4;
    for (uint8_t i = 0; i < n; i++) {
        fc.days[i].code = codes[i] | 0;
        fc.days[i].tMax = tmax[i] | 0.0f;
        fc.days[i].tMin = tmin[i] | 0.0f;

        // 由日期字符串（YYYY-MM-DD）计算星期，供天气页显示"周X"
        const char *dateStr = times[i] | "";
        if (strlen(dateStr) >= 10) {
            struct tm tmv = {};
            tmv.tm_year = atoi(dateStr) - 1900;
            tmv.tm_mon  = atoi(dateStr + 5) - 1;
            tmv.tm_mday = atoi(dateStr + 8);
            tmv.tm_hour = 12;   // 用中午避免时区边界问题
            if (mktime(&tmv) > 0) {
                fc.days[i].wday = (int8_t)tmv.tm_wday;
                Serial.printf("[天气] 第%u天 %s => 星期%d\n",
                              (unsigned)i, dateStr, (int)tmv.tm_wday);
            }
        }
    }
    fc.valid = (n == 4);

    Serial.printf("[天气] %d°C，天气码 %u；4 日预报%s\n",
                  (int)lroundf(out.temperature), (unsigned)out.code,
                  fc.valid ? "已更新" : "数据不全");
    return true;
}

// URL 编码（UTF-8 百分号编码，用于中文地名）
static String urlEncode(const String& input)
{
    static const char *hex = "0123456789ABCDEF";
    String out;
    out.reserve(input.length() * 3);
    for (size_t i = 0; i < input.length(); i++) {
        uint8_t c = (uint8_t)input[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

bool geocodeLocation(const AppConfig& cfg, float& lat, float& lon)
{
    if (cfg.location.length() == 0) return false;

    // 1) 优先使用 NVS 缓存（同一地区不重复请求）
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    String cachedName = prefs.getString("locName", "");
    float cachedLat = prefs.getFloat("locLat", 0.0f);
    float cachedLon = prefs.getFloat("locLon", 0.0f);
    prefs.end();

    if (cachedName == cfg.location && cachedLat != 0.0f) {
        lat = cachedLat;
        lon = cachedLon;
        Serial.printf("[定位] 使用缓存：%s => %.4f, %.4f\n",
                      cfg.location.c_str(), lat, lon);
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) return false;

    // 2) 调用 Open-Meteo 地理编码接口
    String url = "https://geocoding-api.open-meteo.com/v1/search"
                 "?count=1&language=zh&format=json&name="
                 + urlEncode(cfg.location);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("[定位] 请求失败，HTTP %d\n", status);
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument filter;
    filter["results"][0]["latitude"]  = true;
    filter["results"][0]["longitude"] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err || doc["results"][0]["latitude"].isNull()) {
        Serial.printf("[定位] 解析失败或未找到：%s\n", cfg.location.c_str());
        return false;
    }

    lat = doc["results"][0]["latitude"] | 0.0f;
    lon = doc["results"][0]["longitude"] | 0.0f;

    // 3) 写入缓存
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("locName", cfg.location);
    prefs.putFloat("locLat", lat);
    prefs.putFloat("locLon", lon);
    prefs.end();

    Serial.printf("[定位] %s => %.4f, %.4f（已缓存）\n",
                  cfg.location.c_str(), lat, lon);
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

    // 仅 1xx~3xx 视为可达；4xx / 5xx 与无响应均算不可达
    bool ok = (status > 0 && status < 400);
    return ok;
}

// 把窗口推进到当前槽：过期槽整体移出并清零
static void availSlideWindow(Availability& a, int32_t curSlot)
{
    int32_t target = curSlot - (int32_t)g_windowSlots + 1;
    if (target <= a.baseSlot) return;   // 窗口尚未过期

    int32_t shift = target - a.baseSlot;
    if (shift >= (int32_t)g_windowSlots) {
        memset(a.slots, 0, g_windowSlots);   // 长时间未采样，整个窗口作废
    } else {
        memmove(a.slots, a.slots + shift, g_windowSlots - shift);
        memset(a.slots + g_windowSlots - shift, 0, shift);
    }
    a.baseSlot = target;
}

void availabilityRecord(uint8_t index, bool ok)
{
    if (index >= 2) return;
    if (!timeIsValid()) return;   // 未校时不写槽，避免污染时间轴

    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    if (hourInSleep((uint8_t)lt.tm_hour)) return;   // 夜间时段不采样

    int32_t curSlot = (int32_t)(now / g_slotSeconds);
    Availability& a = g_availability[index];
    availSlideWindow(a, curSlot);

    int32_t pos = curSlot - a.baseSlot;
    if (pos < 0 || pos >= (int32_t)g_windowSlots) return;   // 防御：窗口异常

    a.slots[pos] = ok ? AVAIL_SLOT_OK : AVAIL_SLOT_FAIL;
    availabilitySave(index);   // 立即持久化，掉电不丢
}

uint8_t availabilityPercent(uint8_t index)
{
    if (index >= 2 || !timeIsValid()) return 255;

    const Availability& a = g_availability[index];
    int32_t curSlot = availabilitySlotNow();
    int32_t from    = curSlot - (int32_t)g_windowSlots + 1;

    uint16_t good = 0, total = 0;
    for (int32_t s = from; s <= curSlot; s++) {
        int32_t pos = s - a.baseSlot;
        if (pos < 0 || pos >= (int32_t)g_windowSlots) continue;
        uint8_t v = a.slots[pos];
        if (v == AVAIL_SLOT_OK) { good++; total++; }
        else if (v == AVAIL_SLOT_FAIL) { total++; }
    }
    if (total == 0) return 255;   // 暂无数据
    return (uint8_t)((uint32_t)good * 100 / total);
}

int32_t availabilitySlotNow()
{
    return (int32_t)(time(nullptr) / g_slotSeconds);
}

uint8_t availabilitySlotAt(uint8_t index, int32_t slot)
{
    if (index >= 2) return AVAIL_SLOT_NONE;

    const Availability& a = g_availability[index];
    int32_t pos = slot - a.baseSlot;
    if (pos < 0 || pos >= (int32_t)g_windowSlots) return AVAIL_SLOT_NONE;
    return a.slots[pos];
}

uint16_t availabilityTimeline(uint8_t index, uint8_t *out, uint16_t maxLen)
{
    if (index >= 2 || out == nullptr || !timeIsValid()) return 0;

    int32_t curSlot = availabilitySlotNow();
    int32_t from    = curSlot - (int32_t)g_windowSlots + 1;

    uint16_t n = 0;
    for (int32_t s = from; s <= curSlot && n < maxLen; s++) {
        // 夜间时段不占时间轴位置，直接跳过
        time_t t = (time_t)s * g_slotSeconds;
        struct tm lt;
        localtime_r(&t, &lt);
        if (hourInSleep((uint8_t)lt.tm_hour)) continue;
        out[n++] = availabilitySlotAt(index, s);
    }
    return n;
}

void latencyRecord(uint8_t index, bool ok, uint32_t latencyMs)
{
    if (index >= 2) return;

    LatencyHistory& h = g_latency[index];
    uint16_t value = 0xFFFF;   // 失败样本
    if (ok) {
        if (latencyMs > 65000UL) latencyMs = 65000UL;
        value = (uint16_t)latencyMs;
    }

    h.ms[h.next] = value;
    h.next = (uint16_t)((h.next + 1) % LAT_MAX_SAMPLES);
    if (h.count < LAT_MAX_SAMPLES) h.count++;

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    char key[8];
    snprintf(key, sizeof(key), "lat%u", index);
    prefs.putBytes(key, &h, sizeof(LatencyHistory));
    prefs.end();
}

uint16_t latencyHistory(uint8_t index, uint16_t *out, uint16_t maxLen)
{
    if (index >= 2 || out == nullptr) return 0;

    const LatencyHistory& h = g_latency[index];
    uint16_t n = h.count;
    if (n > LAT_MAX_SAMPLES) n = LAT_MAX_SAMPLES;
    if (n > maxLen) n = maxLen;

    for (uint16_t i = 0; i < n; i++) {
        uint16_t pos = (uint16_t)((h.next + LAT_MAX_SAMPLES - n + i) % LAT_MAX_SAMPLES);
        out[i] = h.ms[pos];
    }
    return n;
}
