/*
 * 应用配置：由编译期嵌入的 config.json 解析而来
 * config.json 含敏感信息（WiFi、地区、检测链接），不提交到 git
 */
#pragma once

#include <Arduino.h>

// 单条可用性检测目标
struct CheckTarget {
    String name;   // 屏幕显示名称（域名）
    String url;    // 检测地址（http/https）
};

// 应用配置
struct AppConfig {
    String   wifiSsid;
    String   wifiPassword;
    String   ntpServer;
    int8_t   timezoneOffsetHours;    // 时区偏移（小时）
    float    latitude;               // 纬度（天气接口用）
    float    longitude;              // 经度（天气接口用）
    uint8_t  sleepStartHour;         // 休眠开始（小时）
    uint8_t  sleepEndHour;           // 休眠结束（小时）
    uint16_t refreshSeconds;         // 屏幕刷新周期（秒）
    uint16_t checkIntervalMinutes;   // 可用性检测间隔（分钟）
    uint8_t  statsDays;              // 可用率统计窗口（天）
    uint16_t fullRefreshMinutes;     // 全刷间隔（分钟，用于消除残影）
    uint16_t weatherRefreshMinutes;  // 天气更新间隔（分钟）
    CheckTarget checks[2];           // 最多支持 2 个检测目标
    uint8_t  checkCount;
};

// 解析嵌入固件的 config.json；成功返回 true
bool loadAppConfig(AppConfig& cfg);
