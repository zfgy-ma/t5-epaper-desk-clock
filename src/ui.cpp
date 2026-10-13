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
    if (code >= 51 && code <= 57) return "小雨";
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
            bool fullRefresh, bool night)
{
    epd.fillScreen(GxEPD_WHITE);

    // 外框
    epd.drawRect(0, 0, SCREEN_W, SCREEN_H, GxEPD_BLACK);
    epd.drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, GxEPD_BLACK);

    char buf[40];
    bool timeOk = timeIsValid();

    if (night) {
        // 夜间休眠画面：时间位置改为提示文字
        u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        u8g2.setForegroundColor(GxEPD_BLACK);
        const char *msg = "夜间休眠中";
        int16_t w = u8g2.getUTF8Width(msg);
        u8g2.setCursor((SCREEN_W - w) / 2, 42);
        u8g2.print(msg);

        snprintf(buf, sizeof(buf), "将于 %d:00 自动唤醒", (int)cfg.sleepEndHour);
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        w = u8g2.getUTF8Width(buf);
        u8g2.setCursor((SCREEN_W - w) / 2, 64);
        u8g2.print(buf);
    } else {
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
    }

    // ---- 天气（图标 + 中文描述 + 温度）----
    if (weather.valid) {
        drawWeatherIcon(weather.code, 164, 30);

        // 中文天气描述
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        const char *desc = weatherText(weather.code);
        u8g2.setCursor(192, 40);
        u8g2.print(desc);

        // 温度数字（大号），紧跟在描述之后，避免重叠
        int16_t xTemp = 192 + u8g2.getUTF8Width(desc) + 4;
        snprintf(buf, sizeof(buf), "%d", (int)lroundf(weather.temperature));
        u8g2.setFont(u8g2_font_helvB14_tf);
        u8g2.setCursor(xTemp, 40);
        u8g2.print(buf);

        // 手绘度数小圆圈（避免中文字体缺 ° 字形）
        int16_t tw = u8g2.getUTF8Width(buf);
        epd.drawCircle(xTemp + tw + 3, 28, 2, GxEPD_BLACK);
    } else {
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(194, 40);
        u8g2.print("暂无天气");
    }

    // ---- 地区名称（显示在天气下方）----
    if (cfg.location.length() > 0) {
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        int16_t wLoc = u8g2.getUTF8Width(cfg.location.c_str());
        u8g2.setCursor(198 - wLoc / 2, 60);
        u8g2.print(cfg.location);
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

// 居中绘制温度数字（右上角带度数小圆圈）
static void drawTempCentered(int16_t cx, int16_t baseline, int value,
                             const uint8_t *font, int16_t dotRadius)
{
    char text[8];
    snprintf(text, sizeof(text), "%d", value);
    u8g2.setFont(font);
    int16_t w = u8g2.getUTF8Width(text);
    int16_t x = cx - w / 2;
    u8g2.setCursor(x, baseline);
    u8g2.print(text);
    epd.drawCircle(x + w + 3, baseline - 10, dotRadius, GxEPD_BLACK);
}

void uiDrawForecast(const AppConfig& cfg, const ForecastInfo& fc,
                    bool fullRefresh)
{
    epd.fillScreen(GxEPD_WHITE);
    epd.drawRect(0, 0, SCREEN_W, SCREEN_H, GxEPD_BLACK);
    epd.drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, GxEPD_BLACK);

    // 四列：昨天 / 今天 / 明天 / 后天（竖线分割）
    static const char *labels[4] = { "昨天", "今天", "明天", "后天" };
    const int16_t colW = (SCREEN_W - 4) / 4;

    for (int i = 1; i < 4; i++) {
        epd.drawFastVLine(2 + i * colW, 4, SCREEN_H - 8, GxEPD_BLACK);
    }

    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    for (uint8_t i = 0; i < 4; i++) {
        int16_t cx = 2 + i * colW + colW / 2;

        int16_t w = u8g2.getUTF8Width(labels[i]);
        u8g2.setCursor(cx - w / 2, 22);
        u8g2.print(labels[i]);

        if (!fc.valid) continue;

        drawWeatherIcon(fc.days[i].code, cx, 48);
        drawTempCentered(cx, 92, (int)lroundf(fc.days[i].tMax),
                         u8g2_font_helvB14_tf, 2);   // 最高温度
        drawTempCentered(cx, 112, (int)lroundf(fc.days[i].tMin),
                         u8g2_font_helvB10_tf, 1);   // 最低温度
    }

    if (!fc.valid) {
        const char *msg = "暂无预报数据";
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        int16_t w = u8g2.getUTF8Width(msg);
        u8g2.setCursor((SCREEN_W - w) / 2, 76);
        u8g2.print(msg);
    }

    epd.display(!fullRefresh);
}

void uiDrawLatency(const AppConfig& cfg, uint8_t index,
                   const uint16_t *samples, uint16_t count,
                   bool fullRefresh)
{
    epd.fillScreen(GxEPD_WHITE);
    epd.drawRect(0, 0, SCREEN_W, SCREEN_H, GxEPD_BLACK);
    epd.drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_BLACK);

    // 标题
    char title[40];
    const char *name = (index < cfg.checkCount) ? cfg.checks[index].name.c_str()
                                                : "站点";
    snprintf(title, sizeof(title), "%s 延迟", name);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setCursor(10, 20);
    u8g2.print(title);

    // 统计有效样本
    uint16_t vmin = 0xFFFF, vmax = 0, vcnt = 0, fails = 0;
    uint32_t vsum = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (samples[i] == 0xFFFF) { fails++; continue; }
        if (samples[i] < vmin) vmin = samples[i];
        if (samples[i] > vmax) vmax = samples[i];
        vsum += samples[i];
        vcnt++;
    }

    // 图表区域与坐标轴
    const int16_t x0 = 12, x1 = SCREEN_W - 10, yTop = 30, yBot = 100;
    epd.drawFastVLine(x0, yTop, yBot - yTop, GxEPD_BLACK);
    epd.drawFastHLine(x0, yBot, x1 - x0, GxEPD_BLACK);

    uint16_t yMax = vcnt ? vmax : 100;
    if (yMax < 100) yMax = 100;
    else if (yMax < 1000) yMax = (uint16_t)(((yMax + 99) / 100) * 100);
    else yMax = (uint16_t)(((yMax + 499) / 500) * 500);

    char label[16];
    snprintf(label, sizeof(label), "%ums", (unsigned)yMax);
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.setCursor(x0 + 2, yTop + 8);
    u8g2.print(label);

    for (int i = 1; i <= 3; i++) {
        int16_t y = yBot - (int16_t)((int32_t)(yBot - yTop) * i / 4);
        for (int16_t x = x0 + 2; x < x1; x += 4) {
            epd.drawPixel(x, y, GxEPD_BLACK);
        }
    }

    // 折线（失败样本：顶部画叉并断开）
    if (count > 0) {
        int16_t step = (count > 1) ? (int16_t)((x1 - x0) / (count - 1)) : 0;
        int16_t prevX = -1, prevY = -1;
        for (uint16_t i = 0; i < count; i++) {
            int16_t x = x0 + (int16_t)(i * step);
            if (samples[i] == 0xFFFF) {
                epd.drawLine(x - 2, yTop + 2, x + 2, yTop + 6, GxEPD_BLACK);
                epd.drawLine(x + 2, yTop + 2, x - 2, yTop + 6, GxEPD_BLACK);
                prevX = -1;
                continue;
            }
            int16_t y = yBot - (int16_t)((int32_t)samples[i] * (yBot - yTop) / yMax);
            if (y < yTop) y = yTop;
            if (prevX >= 0) epd.drawLine(prevX, prevY, x, y, GxEPD_BLACK);
            prevX = x;
            prevY = y;
        }
    }

    // 底部统计（纯 ASCII，使用小号字体）
    char stats[56];
    if (vcnt > 0) {
        snprintf(stats, sizeof(stats), "min %u  avg %u  max %u ms  fail %u",
                 (unsigned)vmin, (unsigned)(vsum / vcnt), (unsigned)vmax,
                 (unsigned)fails);
    } else {
        snprintf(stats, sizeof(stats), "no latency data yet");
    }
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.setCursor(10, SCREEN_H - 8);
    u8g2.print(stats);

    epd.display(!fullRefresh);
}
