/*
 * 电池电量：GPIO35 读取 VBAT 的 1/2 分压，换算成大概电量
 *
 * 只用于在主页右上角画电池图标，精度不追求很高。
 */
#pragma once

#include <Arduino.h>

// 初始化 ADC（引脚衰减 11dB，覆盖 0～3.3V 量程）
void batteryInit();

// 读取一次电量并缓存（多次采样取平均，约 5ms）
void batterySample();

// 最近一次采样的电量：0～100；未初始化或读数异常时返回 255
uint8_t batteryPercent();

// 最近一次采样的电池电压（毫伏）；读数异常时返回 0
uint16_t batteryMillivolts();
