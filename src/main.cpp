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

// 用户按键：GPIO39（按下为低电平），用于切换页面
#define BUTTON_PIN (39)

static AppConfig   g_cfg;
static WeatherInfo g_weather;
static ForecastInfo g_forecast;
static uint32_t    g_lastWeatherMs = 0;
static uint32_t    g_lastFullMs    = 0;
static uint32_t    g_lastCheckMs   = 0;
static uint8_t     g_page          = 0;      // 当前页面：0=主页 1=天气 2/3=延迟图
static uint8_t     g_btnLast       = HIGH;   // 按键上次电平

// 检测按键是否被按下（上升沿去抖，返回 true 表示刚按下）
static bool buttonPressed()
{
    uint8_t now = (uint8_t)digitalRead(BUTTON_PIN);
    bool pressed = (g_btnLast == HIGH && now == LOW);
    g_btnLast = now;
    if (pressed) {
        delay(120);   // 简单消抖：等待电平稳定，避免一次按压触发多次翻页
        g_btnLast = (uint8_t)digitalRead(BUTTON_PIN);
    }
    return pressed;
}

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

    // 睡前画面：时间位置显示"夜间休眠中"（全刷后进入深度睡眠）
    g_page = 0;
    uint8_t pct[2] = { availabilityPercent(0), availabilityPercent(1) };
    struct tm t2;
    localtime_r(&now, &t2);
    uiDraw(g_cfg, t2, g_weather, pct, true, true);

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
        latencyRecord(i, ok, latency);
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

// 绘制当前页（full=true 全刷，false 快刷）
static void drawPage(bool full)
{
    uint8_t pct[2] = { availabilityPercent(0), availabilityPercent(1) };

    if (g_page == 1) {
        uiDrawForecast(g_cfg, g_forecast, full);
        return;
    }
    if (g_page == 2 || g_page == 3) {
        uint8_t idx = (uint8_t)(g_page - 2);
        static uint16_t samples[48];
        uint16_t n = latencyHistory(idx, samples, 48);
        uiDrawLatency(g_cfg, idx, samples, n, full);
        return;
    }

    struct tm t;
    localTime(t);
    uiDraw(g_cfg, t, g_weather, pct, full);
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

    // 统计窗口样本数 = 天数 × (24×60 ÷ 检测间隔分钟)
    uint32_t samples = (uint32_t)g_cfg.statsDays * 24UL * 60UL /
                       (g_cfg.checkIntervalMinutes ? g_cfg.checkIntervalMinutes : 15);
    if (samples == 0) samples = 1;
    if (samples > AVAIL_MAX_SAMPLES) samples = AVAIL_MAX_SAMPLES;
    availabilityInit((uint16_t)samples);

    uiInit();

    // 按键初始化（GPIO39，按下为低电平）
    pinMode(BUTTON_PIN, INPUT);
    g_btnLast = (uint8_t)digitalRead(BUTTON_PIN);

    if (wifiEnsure(g_cfg, 15000)) {
        timeStartSync(g_cfg);

        // 地区名称 → 经纬度（带 NVS 缓存，失败时回退到 config 坐标）
        if (g_cfg.location.length() > 0) {
            float lat = g_cfg.latitude, lon = g_cfg.longitude;
            if (geocodeLocation(g_cfg, lat, lon)) {
                g_cfg.latitude  = lat;
                g_cfg.longitude = lon;
            }
        }

        for (int i = 0; i < 50 && !timeIsValid(); i++) delay(200);
        if (weatherFetch(g_cfg, g_weather, g_forecast)) g_lastWeatherMs = millis();
    }
    if (WiFi.status() == WL_CONNECTED) {
        runChecks();
        g_lastCheckMs = millis();
    }

    struct tm t;
    localTime(t);
    if (isSleepTime(t)) sleepUntilMorning();

    drawPage(true);   // 启动时全刷
    g_lastFullMs = millis();
}

void loop()
{
    // 等待到下一个刷新周期边界（对齐整分钟）；期间轮询按键翻页
    uint32_t period = g_cfg.refreshSeconds ? g_cfg.refreshSeconds : 60;
    time_t now = time(nullptr);
    uint32_t wait = period - (uint32_t)(now % (time_t)period);
    if (wait < 1 || wait > period) wait = period;

    uint32_t target = wait * 1000UL;
    for (uint32_t waited = 0; waited < target; waited += 50) {
        if (buttonPressed()) {
            g_page = (uint8_t)((g_page + 1) % 4);
            Serial.printf("[按键] 切换到第 %u 页\n", (unsigned)g_page);
            drawPage(true);             // 翻页用全刷，避免残影
            g_lastFullMs = millis();
        }
        delay(50);
    }

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
        if (weatherFetch(g_cfg, g_weather, g_forecast)) g_lastWeatherMs = millis();
    }

    // 可用性检测：按检测间隔执行（默认 15 分钟一次）
    if (WiFi.status() == WL_CONNECTED &&
        (g_lastCheckMs == 0 ||
         millis() - g_lastCheckMs >=
             (uint32_t)g_cfg.checkIntervalMinutes * 60000UL)) {
        runChecks();
        g_lastCheckMs = millis();
    }

    // 定期整屏全刷，消除快刷残影
    bool full = (millis() - g_lastFullMs) >
                (uint32_t)g_cfg.fullRefreshMinutes * 60000UL;

    drawPage(full);
    if (full) g_lastFullMs = millis();
}
