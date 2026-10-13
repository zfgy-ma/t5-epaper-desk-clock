/*
 * 界面绘制：270° 横屏布局（250 × 122）
 * 顶部：时间 + 日期 + 天气图标与文字
 * 底部：两行网站可用性时间轴（名称 + 百分比 + 黑白时间轴）
 */
#pragma once

#include <time.h>

#include "app_config.h"
#include "net_tasks.h"

// 初始化墨水屏（硬件复位、横屏旋转、字体准备）
void uiInit();

// 绘制整屏并刷新
//   percent[2]：窗口可用率 0~100，255 表示暂无数据（显示在标题行右侧）
//   时间轴数据由 availabilityTimeline() 提供（最近 N 天、每槽一个点）
//   fullRefresh：true 整屏全刷（约 4 秒）；false 快刷（约 0.2 秒）
//   night：true 时时间位置显示"夜间休眠中"（用于睡前画面）
void uiDraw(const AppConfig& cfg, const struct tm& now,
            const WeatherInfo& weather, const uint8_t percent[2],
            bool fullRefresh, bool night = false);

// 第 1 页：昨天 + 未来 3 天天气（图标 + 最高/最低温度，竖线分列）
void uiDrawForecast(const AppConfig& cfg, const ForecastInfo& fc,
                    bool fullRefresh);

// 第 2/3 页：指定站点的延迟折线图
//   samples：按时间顺序的延迟样本（0xFFFF 表示失败）
void uiDrawLatency(const AppConfig& cfg, uint8_t index,
                   const uint16_t *samples, uint16_t count,
                   bool fullRefresh);
