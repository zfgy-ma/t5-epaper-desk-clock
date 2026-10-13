"""
生成 README 界面预览图（按固件 ui.cpp 的布局 1:1 绘制，3 倍放大输出）

用法：python tools/render_preview.py
输出：docs/preview.png（250×122 的 3 倍渲染，共 750×366）
仅用于文档展示，使用通用示例域名，不含任何真实业务信息。
"""
import os

from PIL import Image, ImageDraw, ImageFont

SCALE = 3
W, H = 250, 122
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

img = Image.new("RGB", (W * SCALE, H * SCALE), WHITE)
d = ImageDraw.Draw(img)

FONT_DIR = "C:/Windows/Fonts"


def load_font(name, size):
    return ImageFont.truetype(os.path.join(FONT_DIR, name), size * SCALE)


font_big = load_font("arialbd.ttf", 34)    # 时间
font_cjk = load_font("msyh.ttc", 12)       # 中文
font_small = load_font("arial.ttf", 8)     # 域名
font_num = load_font("arialbd.ttf", 10)    # 温度与百分比


def px(v):
    return v * SCALE


def draw_sun(cx, cy, r):
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=BLACK)
    import math
    for a in range(0, 360, 45):
        rad = math.radians(a)
        x1 = cx + math.cos(rad) * (r + 3)
        y1 = cy + math.sin(rad) * (r + 3)
        x2 = cx + math.cos(rad) * (r + 7)
        y2 = cy + math.sin(rad) * (r + 7)
        d.line([px(x1), px(y1), px(x2), px(y2)], fill=BLACK, width=SCALE)


def draw_bar(x, y, w, h, percent):
    d.rounded_rectangle([px(x), px(y), px(x + w), px(y + h)],
                        radius=px(h // 2), outline=BLACK, width=SCALE)
    if percent >= 100:
        d.rounded_rectangle(
            [px(x + 2), px(y + 2), px(x + w - 2), px(y + h - 2)],
            radius=px((h - 4) // 2), fill=BLACK)
    elif percent > 0:
        fill_w = int((w - 6) * percent / 100)
        d.rectangle([px(x + 3), px(y + 3), px(x + 3 + fill_w), px(y + h - 3)],
                    fill=BLACK)


# 外框
d.rectangle([0, 0, px(W) - 1, px(H) - 1], outline=BLACK, width=SCALE)
d.rectangle([px(1), px(1), px(W - 1) - 1, px(H - 1) - 1], outline=BLACK,
            width=SCALE)

# 时间与日期
d.text((px(12), px(10)), "14:16", font=font_big, fill=BLACK)
d.text((px(12), px(50)), "10月13日 周一", font=font_cjk, fill=BLACK)

# 天气：太阳 + 晴 26°
draw_sun(168, 30, 9)
d.text((px(192), px(26)), "晴", font=font_cjk, fill=BLACK)
temp = "26"
d.text((px(218), px(26)), temp, font=font_num, fill=BLACK)
temp_w = d.textlength(temp, font=font_num) / SCALE
d.ellipse([px(218 + temp_w + 3), px(29), px(218 + temp_w + 5), px(31)],
          outline=BLACK, width=SCALE)

# 点阵分隔线
for x in range(10, W - 12, 3):
    d.rectangle([px(x), px(70), px(x + 1), px(70)], fill=BLACK)

# 两列可用性（config.json 的 name 备注，支持中文）
columns = [("示例站点一", 98), ("示例站点二", 98)]
for i, (name, percent) in enumerate(columns):
    x0 = 10 + i * 122
    d.text((px(x0), px(76)), name, font=font_cjk, fill=BLACK)
    draw_bar(x0, 94, 82, 16, percent)
    text = f"{percent}%"
    tw = d.textlength(text, font=font_num) / SCALE
    d.text((px(x0 + 116 - tw), px(98)), text, font=font_num, fill=BLACK)

out_dir = os.path.join(os.path.dirname(__file__), "..", "docs")
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, "preview.png")
img.save(out_path)
print("已生成预览图:", os.path.abspath(out_path))
