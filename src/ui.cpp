#include "ui.h"

#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// 墨水屏引脚（LILYGO T5 2.13 系列）
#define EPD_CS   (5)
#define EPD_DC   (17)
#define EPD_RST  (16)
#define EPD_BUSY (4)

// 屏幕逻辑尺寸（横屏）
static const int16_t SCREEN_W = 250;
static const int16_t SCREEN_H = 122;

// 面板：HINK-E0213A22（SSD1675B 兼容，122×250）
static GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT>
    epd(GxEPD2_213_B73(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// u8g2 字体渲染器（支持中文）
static U8G2_FOR_ADAFRUIT_GFX u8g2;

void uiInit()
{
    epd.init(0);
    epd.setRotation(1);          // 旋转 90°，横屏使用

    u8g2.begin(epd);
    u8g2.setFontMode(1);         // 透明背景
    u8g2.setFontDirection(0);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
}

// 太阳图标：实心圆 + 8 条射线
static void drawSunIcon(int16_t cx, int16_t cy, int16_t r)
{
    epd.fillCircle(cx, cy, r, GxEPD_BLACK);
    for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int16_t x1 = cx + (int16_t)(cosf(rad) * (r + 3));
        int16_t y1 = cy + (int16_t)(sinf(rad) * (r + 3));
        int16_t x2 = cx + (int16_t)(cosf(rad) * (r + 7));
        int16_t y2 = cy + (int16_t)(sinf(rad) * (r + 7));
        epd.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
}

// 云朵图标：三个圆 + 底部横条
static void drawCloudIcon(int16_t cx, int16_t cy)
{
    epd.fillCircle(cx - 7, cy, 6, GxEPD_BLACK);
    epd.fillCircle(cx + 2, cy - 4, 7, GxEPD_BLACK);
    epd.fillCircle(cx + 9, cy, 5, GxEPD_BLACK);
    epd.fillRect(cx - 13, cy, 27, 6, GxEPD_BLACK);
}

// 按 WMO 天气码绘制简易图标
static void drawWeatherIcon(uint8_t code, int16_t cx, int16_t cy)
{
    if (code <= 1) {                         // 晴
        drawSunIcon(cx, cy, 9);
    } else if (code == 2) {                  // 多云
        drawSunIcon(cx - 6, cy - 5, 6);
        drawCloudIcon(cx + 4, cy + 2);
    } else if (code == 3) {                  // 阴
        drawCloudIcon(cx, cy);
    } else if (code == 45 || code == 48) {   // 雾
        drawCloudIcon(cx, cy - 4);
        for (int i = 0; i < 3; i++) {
            epd.drawFastHLine(cx - 12, cy + 6 + i * 4, 24, GxEPD_BLACK);
        }
    } else if ((code >= 71 && code <= 77) ||
               code == 85 || code == 86) {   // 雪
        drawCloudIcon(cx, cy - 4);
        for (int i = -1; i <= 1; i++) {
            int16_t x = cx + i * 9;
            epd.drawLine(x - 2, cy + 8, x + 2, cy + 12, GxEPD_BLACK);
            epd.drawLine(x + 2, cy + 8, x - 2, cy + 12, GxEPD_BLACK);
            epd.drawPixel(x, cy + 7, GxEPD_BLACK);
            epd.drawPixel(x, cy + 13, GxEPD_BLACK);
        }
    } else if (code >= 95) {                 // 雷雨
        drawCloudIcon(cx, cy - 4);
        epd.drawLine(cx - 2, cy + 6, cx + 3, cy + 10, GxEPD_BLACK);
        epd.drawLine(cx + 3, cy + 10, cx - 1, cy + 10, GxEPD_BLACK);
        epd.drawLine(cx - 1, cy + 10, cx + 2, cy + 16, GxEPD_BLACK);
    } else {                                 // 雨 / 阵雨
        drawCloudIcon(cx, cy - 4);
        for (int i = -1; i <= 1; i++) {
            epd.drawLine(cx + i * 8, cy + 7, cx + i * 8 - 2, cy + 13, GxEPD_BLACK);
        }
    }
}

// WMO 天气码 → 中文描述
static const char* weatherText(uint8_t code)
{
    if (code <= 1) return "晴";
    if (code == 2) return "多云";
    if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 57) return "毛毛雨";
    if (code >= 61 && code <= 67) return "雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code == 85 || code == 86) return "阵雪";
    if (code >= 95) return "雷雨";
    return "未知";
}

// 胶囊进度条：外框线条 + 黑色填充（白色为空）
static void drawCapsuleBar(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t percent)
{
    epd.drawRoundRect(x, y, w, h, h / 2, GxEPD_BLACK);
    if (percent == 255) return;                 // 暂无数据：只画外框

    if (percent >= 100) {                       // 满格：整条填充
        epd.fillRoundRect(x + 2, y + 2, w - 4, h - 4, (h - 4) / 2, GxEPD_BLACK);
        return;
    }

    int16_t fillW = (int16_t)((int32_t)(w - 6) * percent / 100);
    if (fillW > 0) {
        epd.fillRect(x + 3, y + 3, fillW, h - 6, GxEPD_BLACK);
    }
}

// 点阵分隔线（用黑白点阵模拟灰色线条）
static void drawDottedDivider(int16_t y)
{
    for (int16_t x = 10; x < SCREEN_W - 12; x += 3) {
        epd.drawPixel(x, y, GxEPD_BLACK);
        epd.drawPixel(x + 1, y, GxEPD_BLACK);
    }
}

void uiDraw(const AppConfig& cfg, const struct tm& now,
            const WeatherInfo& weather, const uint8_t percent[2],
            bool fullRefresh)
{
    epd.fillScreen(GxEPD_WHITE);

    // 外框
    epd.drawRect(0, 0, SCREEN_W, SCREEN_H, GxEPD_BLACK);
    epd.drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, GxEPD_BLACK);

    char buf[40];
    bool timeOk = timeIsValid();

    // ---- 时间（大号数字）----
    if (timeOk) {
        snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    epd.setFont(&FreeSansBold24pt7b);
    epd.setTextColor(GxEPD_BLACK);
    epd.setCursor(12, 46);
    epd.print(buf);

    // ---- 日期（中文小字）----
    static const char* weekCn[] = { "日", "一", "二", "三", "四", "五", "六" };
    if (timeOk) {
        snprintf(buf, sizeof(buf), "%d月%d日 周%s",
                 now.tm_mon + 1, now.tm_mday, weekCn[now.tm_wday]);
    } else {
        snprintf(buf, sizeof(buf), "等待校时");
    }
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setCursor(12, 64);
    u8g2.print(buf);

    // ---- 天气（图标 + 中文描述 + 温度）----
    if (weather.valid) {
        drawWeatherIcon(weather.code, 168, 30);

        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(194, 40);
        u8g2.print(weatherText(weather.code));

        snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.temperature));
        u8g2.setFont(u8g2_font_helvB10_tf);
        u8g2.setCursor(222, 40);
        u8g2.print(buf);

        // 手绘度数小圆圈（避免中文字体缺 ° 字形）
        int16_t tw = u8g2.getUTF8Width(buf);
        epd.drawCircle(222 + tw + 3, 32, 2, GxEPD_BLACK);
    } else {
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(194, 40);
        u8g2.print("暂无天气");
    }

    // ---- 分隔线 ----
    drawDottedDivider(70);

    // ---- 两列可用性信息 ----
    for (uint8_t i = 0; i < 2; i++) {
        int16_t x0 = 10 + i * 122;

        // 站点备注（支持中文，来自 config.json 的 name 字段）
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(x0, 88);
        u8g2.print(i < cfg.checkCount ? cfg.checks[i].name.c_str() : "");

        // 胶囊进度条
        drawCapsuleBar(x0, 94, 82, 16, percent[i]);

        // 百分比（右对齐）
        if (percent[i] == 255) {
            snprintf(buf, sizeof(buf), "--%%");   // 无数据时显示 --%
        } else {
            snprintf(buf, sizeof(buf), "%u%%", (unsigned)percent[i]);
        }
        u8g2.setFont(u8g2_font_helvB10_tf);
        int16_t tw = u8g2.getUTF8Width(buf);
        u8g2.setCursor(x0 + 116 - tw, 106);
        u8g2.print(buf);
    }

    epd.display(!fullRefresh);   // fullRefresh=false 时走快刷（约 0.2 秒）
}
