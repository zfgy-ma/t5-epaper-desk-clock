# 桌面时钟（墨水屏）

基于 ESP32 + 2.13 英寸墨水屏（122×250）的多功能桌面时钟：

- 左上角显示当前时间，下方小字显示日期与星期
- 右上角显示今日天气（图标 + 中文描述 + 温度）
- 下方显示网站1 与网站2最近 3 天的**可用性时间轴**
  （每 15 分钟一个点，黑色=通、留白=不通，夜间时段不占位）
- 默认每分钟刷新一次屏幕；夜间（默认 0:00–7:00）自动深度睡眠

<p align="center">
  <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/preview-home.png" alt="主页界面预览" width="300">
</p>

> 预览图由 `tools/render_preview.py` 按固件布局生成（示例域名为占位数据）。

按键（GPIO39）循环切换页面，另有天气预报页与延迟折线页：

| 天气预报页（昨天 + 3 天） | 延迟折线页（按站点分页） |
| --- | --- |
| <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/preview-weather.png" alt="天气预报页" width="300"> | <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/preview-latency.png" alt="延迟折线页" width="300"> |

## 实物照片

以下照片记录了设备实际运行时的显示效果（墨水屏在自然光下拍摄，受环境光影响略有偏色）：

<p align="center">
  <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/peoduct-home.png" alt="实物照片：主页" width="320">
  <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/product-weather.png" alt="实物照片：天气预报页" width="320">
  <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/product-latency-1.png" alt="实物照片：站点1延迟页" width="320">
  <img src="https://raw.githubusercontent.com/zfgy-ma/Image_Gallery/main/t5-epaper-desk-clock-image/product-latency-2.png" alt="实物照片：站点2延迟页" width="320">
</p>

## 硬件

| 项目 | 说明 |
| --- | --- |
| 主控 | ESP32 开发板（如 LILYGO T5 2.13 系列） |
| 屏幕 | 2.13 英寸墨水屏，122×250，SSD1675B / SSD1680 兼容 |
| 接线 | 墨水屏：CS=5, DC=17, RST=16, BUSY=4（可在 `src/ui.cpp` 修改） |
| 数据线 | Micro USB；串口默认 COM3（可在 `platformio.ini` 修改） |

## 快速开始

1. 安装 [PlatformIO](https://platformio.org/)（VSCode 插件或命令行版）
2. **复制 `example-config.json` 为 `config.json`**
3. **编辑 `config.json`**：填写 WiFi、地区经纬度、检测链接等（见下方说明）
4. 编译并上传固件：

```bash
pio run -t upload
```

5. 查看运行日志：`pio device monitor -b 115200`


## 配置说明（config.json）

| 字段 | 说明 | 示例 |
| --- | --- | --- |
| `wifi_ssid` | WiFi 名称 | `"MyWiFi"` |
| `wifi_password` | WiFi 密码 | `"12345678"` |
| `ntp_server` | NTP 校时服务器 | `"ntp.aliyun.com"` |
| `timezone_offset_hours` | 时区（小时，东八区为 8） | `8` |
| `location` | 地区名称（自动转换为经纬度，支持中文） | `"杭州"` |
| `latitude` / `longitude` | 经纬度（可选，作为地区解析失败时的兜底） | `30.2741` / `120.1551` |
| `sleep_start_hour` | 夜间休眠开始（小时） | `0` |
| `sleep_end_hour` | 夜间休眠结束（小时） | `7` |
| `refresh_seconds` | 屏幕刷新周期（秒） | `60` |
| `check_interval_minutes` | 可用性检测间隔（分钟） | `15` |
| `stats_days` | 时间轴统计窗口（天） | `3` |
| `full_refresh_minutes` | 全刷间隔（分钟，消除快刷残影） | `10` |
| `weather_refresh_minutes` | 天气更新间隔（分钟） | `10` |
| `checks` | 可用性检测目标列表（最多 2 个） | 见示例文件 |

`checks` 每项包含两个字段：

- `name`：屏幕显示的**备注名称（支持中文）**，如"网站1""网站2"；
  网址太长时不建议直接显示，用中文备注更清晰
- `url`：实际检测地址（支持 http / https）

## 工作机制

### 设备操作

- **按键（GPIO39）**：循环切换页面
  `主页 → 天气预报页 → 站点1延迟 → 站点2延迟 → 主页`
- **自动返回**：非主页停留超过 2 分钟自动回到主页
- **夜间休眠**：休眠时段内屏幕显示"夜间休眠中"，完成后到点自动刷新
- **刷新策略**：默认每分钟快刷；按 `full_refresh_minutes`
  （默认 10 分钟）整屏全刷一次以消除残影
- **调试**：串口发送字符 `n` 也可切换页面（无需按键，便于开发验证）

- **时间**：联网后通过 NTP 校时，之后由 ESP32 内部时钟计时
- **定位**：`location`（如"杭州"）经 Open-Meteo 地理编码接口转换为经纬度，
  结果缓存在 NVS 中供后续启动直接复用（失败时回退到 `latitude`/`longitude`）
- **天气**：使用 Open-Meteo 免费接口（无需 API Key），按经纬度查询
- **可用性检测**：按 `check_interval_minutes` 周期对每个 `url` 发起 HTTP(S) 请求
  - 能收到 HTTP 响应（含 4xx）视为"可达"；无响应或 5xx 视为异常
  - 屏幕以**时间轴**显示 `stats_days` 天窗口内的检测结果
    （默认最近 3 天、每 15 分钟一个点）：黑色=通、留白=不通，
    夜间休眠时段不采样、时间轴上不占位
  - 时间轴右侧显示整个窗口的可用率百分比
  - 统计数据写入 NVS 闪存，**掉电、重启后仍然保留**
- **刷新策略**：默认每分钟快刷一次（约 0.2 秒）；
  按 `full_refresh_minutes` 整屏全刷一次（约 4 秒）以消除残影
- **夜间休眠**：在休眠时段内深度睡眠，到 `sleep_end_hour` 自动唤醒；
  墨水屏在休眠期间保持显示、不耗电


## 使用的开源库

| 库 | 用途 | 许可证 |
| --- | --- | --- |
| [GxEPD2](https://github.com/ZinggJM/GxEPD2) | 墨水屏驱动（快刷/全刷） | GPL-3.0 |
| [U8g2_for_Adafruit_GFX](https://github.com/olikraus/U8g2_for_Adafruit_GFX) | 中文/拉丁字体渲染 | MIT |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON 解析（配置与天气） | MIT |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | 图形绘制基础库（GxEPD2 依赖） | BSD |
| [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) | WiFi / HTTPClient / NTP | LGPL-2.1 |

## 常见问题

**屏幕一直显示"等待校时"、可用率显示 `--%`？**

说明设备拿到的时间无效或未联网。检查 `config.json` 里的
`wifi_ssid` / `wifi_password` 是否正确，串口日志会给出连接结果。

**屏幕偶尔有残影？**

正常现象：快刷会累积轻微残影，程序会按 `full_refresh_minutes`
定期整屏全刷（默认 60 分钟）清除。

**刷新时屏幕会闪一下？**

这是墨水屏整屏全刷的正常表现（默认每 10 分钟一次），用于保持显示质量。

**想调整夜间休眠时间？**

修改 `config.json` 的 `sleep_start_hour` / `sleep_end_hour`，
支持跨零点时段（例如 23 点至次日 7 点）。

**断电后可用性数据会丢失吗？**

不会。统计数据保存在 NVS 闪存中，掉电、重启后都会继续累计
（仅恢复出厂设置或手动清空 NVS 才会重置）。

**面板是黑白的，为什么设计图是四灰度？**

本项目使用的 122×250 面板为黑白（1 位/像素）；需要灰色观感时可使用
点阵抖动（dither）模拟，界面中的点状分隔线即为此类处理。

## 许可

本项目基于 **GPL-3.0** 许可发布（依赖 GPL-3.0 的 GxEPD2），详见 `LICENSE`。
