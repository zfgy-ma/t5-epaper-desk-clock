/*
 * 界面绘制：270° 横屏布局（250 × 122）
 * 顶部：时间 + 日期 + 天气图标与文字
 * 底部：两列网站可用性（名称 + 胶囊进度条 + 百分比）
 */
#pragma once

#include <time.h>

#include "app_config.h"
#include "net_tasks.h"

// 初始化墨水屏（硬件复位、横屏旋转、字体准备）
void uiInit();

// 绘制整屏并刷新
//   percent[2]：可用率 0~100，255 表示暂无数据
//   fullRefresh：true 整屏全刷（约 4 秒）；false 快刷（约 0.2 秒）
void uiDraw(const AppConfig& cfg, const struct tm& now,
            const WeatherInfo& weather, const uint8_t percent[2],
            bool fullRefresh);
