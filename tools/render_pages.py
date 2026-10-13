"""
生成新增页面（天气预报页 / 延迟折线页）的预览图

用法：python tools/render_pages.py
输出：docs/preview-weather.png、docs/preview-latency.png
仅用于文档展示，数据为示例数据。
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

SCALE = 3
W, H = 250, 122
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
FONT_DIR = "C:/Windows/Fonts"


def load_font(name, size):
    return ImageFont.truetype(os.path.join(FONT_DIR, name), size * SCALE)


font_cjk = load_font("msyh.ttc", 12)
font_small = load_font("arial.ttf", 8)
font_mid = load_font("arialbd.ttf", 10)
font_big = load_font("arialbd.ttf", 14)


def px(v):
    return round(v * SCALE)


def new_page():
    img = Image.new("RGB", (W * SCALE, H * SCALE), WHITE)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, px(W) - 1, px(H) - 1], outline=BLACK, width=SCALE)
    d.rectangle([px(1), px(1), px(W - 1) - 1, px(H - 1) - 1],
                outline=BLACK, width=SCALE)
    return img, d


def draw_sun(d, cx, cy, r=9):
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=BLACK)
    for a in range(0, 360, 45):
        rad = math.radians(a)
        d.line([px(cx + math.cos(rad) * (r + 3)), px(cy + math.sin(rad) * (r + 3)),
                px(cx + math.cos(rad) * (r + 7)), px(cy + math.sin(rad) * (r + 7))],
               fill=BLACK, width=SCALE)


def draw_cloud(d, cx, cy):
    d.ellipse([px(cx - 13), px(cy - 6), px(cx - 1), px(cy + 6)], fill=BLACK)
    d.ellipse([px(cx - 5), px(cy - 11), px(cx + 9), px(cy + 3)], fill=BLACK)
    d.ellipse([px(cx + 4), px(cy - 5), px(cx + 14), px(cy + 5)], fill=BLACK)
    d.rectangle([px(cx - 13), px(cy), px(cx + 14), px(cy + 6)], fill=BLACK)


def temp_text(d, cx, baseline, value, font):
    t = str(value)
    w = d.textlength(t, font=font) / SCALE
    x = cx - w / 2
    d.text((px(x), px(baseline - 13)), t, font=font, fill=BLACK)
    d.ellipse([px(x + w + 3), px(baseline - 13), px(x + w + 5), px(baseline - 11)],
              outline=BLACK, width=SCALE)


# ---- 页面 1：天气预报（昨天 / 今天 / 明天 / 后天）----
img, d = new_page()
labels = ["昨天", "今天", "周一", "周二"]
codes = ["rain", "sun", "cloud", "sun"]
tmax = [26, 28, 25, 24]
tmin = [17, 16, 18, 17]
col_w = (W - 4) // 4

for i in range(1, 4):
    x = 2 + i * col_w
    d.line([px(x), px(4), px(x), px(H - 4)], fill=BLACK, width=SCALE)

for i in range(4):
    cx = 2 + i * col_w + col_w // 2
    lw = d.textlength(labels[i], font=font_cjk) / SCALE
    d.text((px(cx - lw / 2), px(8)), labels[i], font=font_cjk, fill=BLACK)

    if codes[i] == "sun":
        draw_sun(d, cx, 48)
    elif codes[i] == "cloud":
        draw_cloud(d, cx, 48)
    else:
        draw_cloud(d, cx, 44)
        for k in (-1, 0, 1):
            d.line([px(cx + k * 8), px(56), px(cx + k * 8 - 2), px(62)],
                   fill=BLACK, width=SCALE)

    temp_text(d, cx, 92, tmax[i], font_big)
    temp_text(d, cx, 112, tmin[i], font_mid)

out_dir = os.path.join(os.path.dirname(__file__), "..", "docs")
img.save(os.path.join(out_dir, "preview-weather.png"))

# ---- 页面 2/3：延迟折线图 ----
img, d = new_page()
d.text((px(10), px(8)), "示例站点一 延迟", font=font_cjk, fill=BLACK)

x0, x1, ytop, ybot = 12, W - 10, 30, 100
d.line([px(x0), px(ytop), px(x0), px(ybot)], fill=BLACK, width=SCALE)
d.line([px(x0), px(ybot), px(x1), px(ybot)], fill=BLACK, width=SCALE)
d.text((px(x0 + 2), px(ytop)), "3000ms", font=font_small, fill=BLACK)

# 3 条点阵网格线
for i in (1, 2, 3):
    y = ybot - (ybot - ytop) * i / 4
    xx = x0 + 2
    while xx < x1:
        d.rectangle([px(xx), px(y), px(xx + 1), px(y)], fill=BLACK)
        xx += 4

# 示例延迟序列（None 表示检测失败）
samples = [1200, 1500, 1800, 1400, None, 2200, 2600, 2400, 2000, 1800, 3000, 2800,
           2500, 2300, None, 2100, 1900, 1700, 2200, 2500, 2700, 2900, 2600, 2400,
           2200, 2000, 1800, 1600, 2100, 2300, 2500, 2200, 2000, 1800, 1900, 2100,
           2400, 2600, 2800, 2500, 2300, 2100, 2200, 2400, 2600, 2800, 3000, 2800]
ymax = 3000
step = (x1 - x0) / (len(samples) - 1)
prev = None
for i, v in enumerate(samples):
    x = x0 + i * step
    if v is None:
        d.line([px(x - 2), px(ytop + 2), px(x + 2), px(ytop + 6)],
               fill=BLACK, width=SCALE)
        d.line([px(x + 2), px(ytop + 2), px(x - 2), px(ytop + 6)],
               fill=BLACK, width=SCALE)
        prev = None
        continue
    y = ybot - v * (ybot - ytop) / ymax
    if prev:
        d.line([px(prev[0]), px(prev[1]), px(x), px(y)], fill=BLACK, width=SCALE)
    d.ellipse([px(x - 1), px(y - 1), px(x + 1), px(y + 1)], fill=BLACK)
    prev = (x, y)

stats1 = "min1200 avg2264"
stats2 = "max3000 fail2"
sw1 = d.textlength(stats1, font=font_small) / SCALE
d.text((px(W - 10 - sw1), px(6)), stats1, font=font_small, fill=BLACK)
sw2 = d.textlength(stats2, font=font_small) / SCALE
d.text((px(W - 10 - sw2), px(16)), stats2, font=font_small, fill=BLACK)

d.text((px(x0), px(H - 13)), "13:00", font=font_small, fill=BLACK)
tw = d.textlength("16:00", font=font_small) / SCALE
d.text((px(x1 - tw), px(H - 13)), "16:00", font=font_small, fill=BLACK)
img.save(os.path.join(out_dir, "preview-latency.png"))

print("已生成: docs/preview-weather.png, docs/preview-latency.png")
