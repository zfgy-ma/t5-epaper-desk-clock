#include "battery.h"

// 电量检测引脚：VBAT 经 R42/R43（100K/100K）分压到 GPIO35
#define BATTERY_PIN (35)

// 分压比：两枚 100K 等值电阻，ADC 读到的电压是电池电压的一半
static const float BATTERY_DIVIDER = 2.0f;

// 采样次数与丢弃个数（丢弃上电瞬间前几次不稳定的读数）
static const uint8_t SAMPLE_COUNT = 16;
static const uint8_t SAMPLE_DROP = 2;

static uint8_t  s_percent = 255;   // 255 表示还没有有效读数
static uint16_t s_millivolts = 0;

// 锂电池放电曲线：电压（mV）→ 剩余电量（%）
// 取 3.7V 单节锂电的典型近似曲线，不追求精确，只求图标大致正确。
// 3.30V 以下视为接近耗尽，4.15V 以上视为满电。
static const struct {
    uint16_t mv;
    uint8_t  pct;
} CURVE[] = {
    { 3300,   0 },   // 保护板即将切断
    { 3500,  10 },   // 低电量提醒区间
    { 3650,  30 },
    { 3750,  50 },
    { 3850,  65 },
    { 3950,  80 },
    { 4050,  92 },
    { 4150, 100 },
};

// 按放电曲线做分段线性插值
static uint8_t percentFromMillivolts(uint16_t mv)
{
    const uint8_t n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv <= CURVE[0].mv) return 0;
    if (mv >= CURVE[n - 1].mv) return 100;

    for (uint8_t i = 1; i < n; i++) {
        if (mv <= CURVE[i].mv) {
            uint16_t lo = CURVE[i - 1].mv, hi = CURVE[i].mv;
            uint8_t plo = CURVE[i - 1].pct, phi = CURVE[i].pct;
            uint32_t num = (uint32_t)(mv - lo) * (phi - plo);
            return (uint8_t)(plo + num / (hi - lo));
        }
    }
    return 100;
}

void batteryInit()
{
    pinMode(BATTERY_PIN, INPUT);
    // 11dB 衰减：ADC 可测到约 3.1V，覆盖 4.2V 电池经分压后的 2.1V
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    analogSetWidth(12);
}

void batterySample()
{
    // 冒泡排序去除两端极值，减少偶发尖峰对平均值的影响
    uint16_t raw[SAMPLE_COUNT];
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        raw[i] = (uint16_t)analogReadMilliVolts(BATTERY_PIN);
        delay(1);
    }
    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        for (uint8_t j = i + 1; j < SAMPLE_COUNT; j++) {
            if (raw[j] < raw[i]) {
                uint16_t t = raw[i];
                raw[i] = raw[j];
                raw[j] = t;
            }
        }
    }

    uint32_t sum = 0;
    uint8_t used = 0;
    for (uint8_t i = SAMPLE_DROP; i < SAMPLE_COUNT - SAMPLE_DROP; i++) {
        sum += raw[i];
        used++;
    }
    if (used == 0) return;

    uint16_t adcMv = (uint16_t)(sum / used);
    // 没接电池时 GPIO35 被拉到地，读数接近 0；此时不画图标，避免误显示 0%
    if (adcMv < 150) {
        s_millivolts = 0;
        s_percent = 255;
        return;
    }
    uint32_t batMv = (uint32_t)(adcMv * BATTERY_DIVIDER);
    s_millivolts = (uint16_t)batMv;
    s_percent = percentFromMillivolts(s_millivolts);
}

uint8_t batteryPercent()
{
    return s_percent;
}

uint16_t batteryMillivolts()
{
    return s_millivolts;
}
