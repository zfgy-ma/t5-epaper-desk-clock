# 桌面时钟（墨水屏）

基于 ESP32 + 2.13 英寸墨水屏（122×250）的多功能桌面时钟：

- 左上角显示当前时间，下方小字显示日期与星期
- 右上角显示今日天气（图标 + 中文描述 + 温度）
- 下方两列显示后台 API 与公司官网的**可用性**（胶囊进度条 + 百分比）
- 默认每分钟刷新一次屏幕；夜间（默认 0:00–7:00）自动深度睡眠

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

> `config.json` 含隐私信息，已在 `.gitignore` 中忽略，**请勿提交到公开仓库**。

## 配置说明（config.json）

| 字段 | 说明 | 示例 |
| --- | --- | --- |
| `wifi_ssid` | WiFi 名称 | `"MyWiFi"` |
| `wifi_password` | WiFi 密码 | `"12345678"` |
| `ntp_server` | NTP 校时服务器 | `"ntp.aliyun.com"` |
| `timezone_offset_hours` | 时区（小时，东八区为 8） | `8` |
| `latitude` / `longitude` | 地区经纬度（天气查询用） | `30.2741` / `120.1551` |
| `sleep_start_hour` | 夜间休眠开始（小时） | `0` |
| `sleep_end_hour` | 夜间休眠结束（小时） | `7` |
| `refresh_seconds` | 屏幕刷新周期（秒） | `60` |
| `full_refresh_minutes` | 全刷间隔（分钟，消除快刷残影） | `60` |
| `weather_refresh_minutes` | 天气更新间隔（分钟） | `10` |
| `checks` | 可用性检测目标列表（最多 2 个） | 见示例文件 |

`checks` 每项包含 `name`（屏幕显示名称）与 `url`（检测地址）。

## 工作机制

- **时间**：联网后通过 NTP 校时，之后由 ESP32 内部时钟计时
- **天气**：使用 Open-Meteo 免费接口（无需 API Key），按经纬度查询
- **可用性检测**：按周期对每个 `url` 发起 HTTP(S) 请求
  - 能收到 HTTP 响应（含 4xx）视为"可达"；无响应或 5xx 视为异常
  - 屏幕显示最近 64 次检测的成功率（滚动窗口，断电后重新统计）
- **刷新策略**：默认每分钟快刷一次（约 0.2 秒）；
  按 `full_refresh_minutes` 整屏全刷一次（约 4 秒）以消除残影
- **夜间休眠**：在休眠时段内深度睡眠，到 `sleep_end_hour` 自动唤醒；
  墨水屏在休眠期间保持显示、不耗电

## 隐私与安全

- `config.json`（WiFi 密码、地区、检测链接）**不会**进入 git，请自行保管
- 天气与检测请求使用 HTTPS；为便于嵌入式端部署，当前用 `setInsecure()`
  跳过证书链校验，适用于公开只读接口；如需更高安全性，可改用
  `setCACert()` 配置根证书
- 公开仓库前请确认：`config.json` 不在提交列表中
  （可用 `git status --ignored` 检查）

## 使用的开源库

| 库 | 用途 | 许可证 |
| --- | --- | --- |
| [GxEPD2](https://github.com/ZinggJM/GxEPD2) | 墨水屏驱动（快刷/全刷） | GPL-3.0 |
| [U8g2_for_Adafruit_GFX](https://github.com/olikraus/U8g2_for_Adafruit_GFX) | 中文/拉丁字体渲染 | MIT |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON 解析（配置与天气） | MIT |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | 图形绘制基础库（GxEPD2 依赖） | BSD |
| [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) | WiFi / HTTPClient / NTP | LGPL-2.1 |

## 许可

本项目基于 **GPL-3.0** 许可发布（依赖 GPL-3.0 的 GxEPD2），详见 `LICENSE`。
