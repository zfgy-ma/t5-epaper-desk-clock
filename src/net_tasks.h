/*
 * 网络任务：WiFi 连接、NTP 校时、天气获取、可用性检测与滚动统计
 */
#pragma once

#include <Arduino.h>
#include <time.h>

#include "app_config.h"

// 天气信息（Open-Meteo 当前天气）
struct WeatherInfo {
    bool     valid       = false;
    float    temperature = 0.0f;
    uint8_t  code        = 0;   // WMO 天气码
    uint32_t updatedMs   = 0;   // 上次更新时间（millis）
};

// 单日预报（用于"昨天 + 3 天"天气页）
struct DailyForecast {
    uint8_t code = 0;    // WMO 天气码
    float   tMax = 0.0f; // 最高温度
    float   tMin = 0.0f; // 最低温度
};

struct ForecastInfo {
    bool          valid = false;
    DailyForecast days[4];   // 顺序：昨天、今天、明天、后天
};

// 可用性统计：环形样本窗口（位图），写入 NVS 后掉电不丢失
// 容量 2048 个样本，足够覆盖"7 天 × 5 分钟一次"的较长窗口
static const uint16_t AVAIL_MAX_SAMPLES = 2048;

struct Availability {
    uint16_t count = 0;      // 已记录样本数（最多 AVAIL_MAX_SAMPLES）
    uint16_t next  = 0;      // 环形缓冲写入位置
    uint32_t total = 0;      // 累计检测次数
    uint32_t ok    = 0;      // 累计成功次数
    uint8_t  bits[AVAIL_MAX_SAMPLES / 8] = { 0 };   // 1=可用，0=不可用
};

// 连接 WiFi（带超时）；已连接时直接返回 true
bool wifiEnsure(const AppConfig& cfg, uint32_t timeoutMs);

// 启动 NTP 校时（后台异步完成）
void timeStartSync(const AppConfig& cfg);

// 当前时间是否已同步有效（年份 ≥ 2020）
bool timeIsValid();

// 拉取天气（当前 + 昨天/未来 3 天）；成功返回 true
bool weatherFetch(const AppConfig& cfg, WeatherInfo& out, ForecastInfo& fc);

// 把地区名称解析成经纬度（Open-Meteo 地理编码，带 NVS 缓存）
// 成功时写回 lat/lon 并返回 true
bool geocodeLocation(const AppConfig& cfg, float& lat, float& lon);

// 检测单个 URL 是否可用；latencyMs 返回耗时
bool urlCheck(const String& url, uint32_t& latencyMs);

// 初始化统计：设置统计窗口样本数，并从 NVS 恢复历史数据
void availabilityInit(uint16_t windowSamples);

// 记录一次检测结果（环形窗口 + 累计），并立即写入 NVS
void availabilityRecord(uint8_t index, bool ok);

// 读取可用率（0~100）；暂无数据返回 255
uint8_t availabilityPercent(uint8_t index);

// 延迟历史样本上限（15 分钟一次时约 36 小时）
static const uint16_t LAT_MAX_SAMPLES = 144;

// 记录一次检测延迟（失败存 0xFFFF），并写入 NVS
void latencyRecord(uint8_t index, bool ok, uint32_t latencyMs);

// 读取最近 maxLen 个延迟样本（按时间顺序），返回实际样本数
uint16_t latencyHistory(uint8_t index, uint16_t *out, uint16_t maxLen);
