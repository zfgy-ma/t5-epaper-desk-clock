/*
 * 桌面时钟 / 天气 / 网站可用性监控
 *
 * 硬件：ESP32 + 2.13 英寸墨水屏（122×250，SSD1675B 兼容）
 * 功能：
 *   - 每分钟刷新一次屏幕（周期可在 config.json 调整）
 *   - 联网获取当前时间（NTP）与天气（Open-Meteo，免密钥）
 *   - 定时检测后台 API 与公司官网的可用性，滚动统计可用率
 *   - 夜间（默认 0 点至 7 点）深度睡眠，清晨自动唤醒
 *
 * 敏感配置（WiFi、地区、检测链接）保存在 config.json，
 * 编译时嵌入固件，该文件不会提交到 git。
 */
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include "app_config.h"
#include "net_tasks.h"
#include "ui.h"

static AppConfig   g_cfg;
static WeatherInfo g_weather;
static uint32_t    g_lastWeatherMs = 0;
static uint32_t    g_lastFullMs    = 0;

// 当前是否处于休眠时段（支持跨零点配置，如 23 点至 7 点）
static bool isSleepTime(const struct tm& t)
{
    if (!timeIsValid()) return false;   // 未校时时不休眠

    uint8_t h = t.tm_hour;
    if (g_cfg.sleepStartHour <= g_cfg.sleepEndHour) {
        return h >= g_cfg.sleepStartHour && h < g_cfg.sleepEndHour;
    }
    return h >= g_cfg.sleepStartHour || h < g_cfg.sleepEndHour;
}

// 深度睡眠，直到休眠时段结束
static void sleepUntilMorning()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    // 计算距离休眠结束时刻的秒数（支持跨零点时段，如 23 点至 7 点）
    int32_t sec = (int32_t)g_cfg.sleepEndHour * 3600
                - (t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
    if (sec <= 0) sec += 24 * 3600;
    if (sec <= 0 || sec > 24 * 3600) sec = 60;   // 异常保护：一分钟后再试

    Serial.printf("[休眠] 深度睡眠约 %.1f 小时后唤醒\n", sec / 3600.0);
    esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
    esp_deep_sleep_start();
}

// 执行一轮可用性检测并记录
static void runChecks()
{
    if (WiFi.status() != WL_CONNECTED) return;   // 自身断网不计入服务失败

    for (uint8_t i = 0; i < g_cfg.checkCount; i++) {
        uint32_t latency = 0;
        bool ok = urlCheck(g_cfg.checks[i].url, latency);
        availabilityRecord(i, ok);
        Serial.printf("[检测] %s = %s（%u ms）\n",
                      g_cfg.checks[i].name.c_str(), ok ? "可用" : "不可用",
                      (unsigned)latency);
    }
}

// 读取当前本地时间
static void localTime(struct tm& t)
{
    time_t now = time(nullptr);
    localtime_r(&now, &t);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[桌面时钟] 启动");

    if (!loadAppConfig(g_cfg)) {
        Serial.println("[桌面时钟] config.json 无效，请检查配置");
        while (true) delay(1000);   // 配置错误时停机等待
    }

    uiInit();

    if (wifiEnsure(g_cfg, 15000)) {
        timeStartSync(g_cfg);
        for (int i = 0; i < 50 && !timeIsValid(); i++) delay(200);
        if (weatherFetch(g_cfg, g_weather)) g_lastWeatherMs = millis();
    }
    runChecks();

    struct tm t;
    localTime(t);
    if (isSleepTime(t)) sleepUntilMorning();

    uint8_t pct[2] = { availabilityPercent(0), availabilityPercent(1) };
    uiDraw(g_cfg, t, g_weather, pct, true);   // 启动时全刷
    g_lastFullMs = millis();
}

void loop()
{
    // 等待到下一个刷新周期边界（对齐整分钟）
    uint32_t period = g_cfg.refreshSeconds ? g_cfg.refreshSeconds : 60;
    time_t now = time(nullptr);
    uint32_t wait = period - (uint32_t)(now % (time_t)period);
    if (wait < 1 || wait > period) wait = period;
    delay(wait * 1000UL);

    struct tm t;
    localTime(t);

    if (isSleepTime(t)) sleepUntilMorning();

    // 断线重连
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiEnsure(g_cfg, 10000) && !timeIsValid()) timeStartSync(g_cfg);
    }

    // 天气按周期更新
    if (WiFi.status() == WL_CONNECTED &&
        (g_lastWeatherMs == 0 ||
         millis() - g_lastWeatherMs >
             (uint32_t)g_cfg.weatherRefreshMinutes * 60000UL)) {
        if (weatherFetch(g_cfg, g_weather)) g_lastWeatherMs = millis();
    }

    if (WiFi.status() == WL_CONNECTED) runChecks();

    // 定期整屏全刷，消除快刷残影
    bool full = (millis() - g_lastFullMs) >
                (uint32_t)g_cfg.fullRefreshMinutes * 60000UL;

    uint8_t pct[2] = { availabilityPercent(0), availabilityPercent(1) };
    uiDraw(g_cfg, t, g_weather, pct, full);
    if (full) g_lastFullMs = millis();
}
