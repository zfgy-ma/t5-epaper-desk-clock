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
    int8_t  wday = -1;   // 星期（0=周日 … 6=周六，-1=未知）
};

struct ForecastInfo {
    bool          valid = false;
    DailyForecast days[4];   // 顺序：昨天、今天、明天、后天
};

// 可用性统计：按检测间隔对齐的绝对时间槽（写入 NVS 后掉电不丢失）
// 渲染时把最近 N 天的槽画成时间轴：黑=通、白=不通，夜间休眠不占位
static const uint16_t AVAIL_MAX_SLOTS = 1024;   // 槽数上限（默认 3 天 × 96 槽）

// 时间槽状态
enum : uint8_t {
    AVAIL_SLOT_NONE = 0,   // 无数据（夜间休眠或未采样）
    AVAIL_SLOT_FAIL = 1,   // 检测不通（时间轴显示为白）
    AVAIL_SLOT_OK   = 2,   // 检测通过（时间轴显示为黑）
};

struct Availability {
    int32_t baseSlot = 0;                    // slots[0] 对应的绝对槽号
    uint8_t slots[AVAIL_MAX_SLOTS] = { 0 };  // 每槽状态（AVAIL_SLOT_xxx）
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

// 初始化统计：windowSlots=时间轴窗口槽数，slotSeconds=每槽秒数，
// sleepStartHour/sleepEndHour=夜间休眠时段（该时段不采样、不统计）
void availabilityInit(uint16_t windowSlots, uint16_t slotSeconds,
                      uint8_t sleepStartHour, uint8_t sleepEndHour);

// 记录一次检测结果到当前时间槽，并立即写入 NVS
// （未校时或处于夜间时段时忽略该结果）
void availabilityRecord(uint8_t index, bool ok);

// 读取最近窗口内的可用率（0~100）；暂无数据返回 255
uint8_t availabilityPercent(uint8_t index);

// 当前时间对应的绝对槽号（time() / 每槽秒数）
int32_t availabilitySlotNow();

// 读取指定绝对槽号的检测状态；不在统计窗口内返回 AVAIL_SLOT_NONE
uint8_t availabilitySlotAt(uint8_t index, int32_t slot);

// 构建时间轴视图：按时间升序返回窗口内"采样时段"的槽状态（跳过夜间）
// out 容量需不小于窗口槽数；返回实际槽数
uint16_t availabilityTimeline(uint8_t index, uint8_t *out, uint16_t maxLen);

// 延迟历史样本上限（15 分钟一次时约 36 小时）
static const uint16_t LAT_MAX_SAMPLES = 144;

// 记录一次检测延迟（失败存 0xFFFF），并写入 NVS
void latencyRecord(uint8_t index, bool ok, uint32_t latencyMs);

// 读取最近 maxLen 个延迟样本（按时间顺序），返回实际样本数
uint16_t latencyHistory(uint8_t index, uint16_t *out, uint16_t maxLen);
