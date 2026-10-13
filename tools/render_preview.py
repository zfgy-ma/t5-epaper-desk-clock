"""
生成 README 界面预览图（按固件 ui.cpp 的布局 1:1 绘制，3 倍放大输出）

用法：python tools/render_preview.py
输出：docs/preview.png（250×122 的 3 倍渲染，共 750×366）
     也可用 python tools/render_preview.py <输出路径> 指定其他位置
仅用于文档展示，使用通用示例域名，不含任何真实业务信息。
"""
import os
import sys

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


def draw_timeline(samples, x, y, w, h):
    """可用性时间轴：True=黑色（通），False=留白（不通）"""
    n = len(samples)
    prev = x
    for i, ok in enumerate(samples):
        nxt = x + int((i + 1) * w / n)
        if ok:
            seg = max(1, nxt - prev)
            d.rectangle([px(prev), px(y), px(prev + seg) - 1, px(y + h) - 1],
                        fill=BLACK)
        prev = nxt


def build_timeline(outages):
    """生成 3 天 × 17 小时 × 4 次 = 204 个点；outages 为不通的区间"""
    total = 204
    samples = [True] * total
    for start, length in outages:
        for k in range(start, min(start + length, total)):
            samples[k] = False
    return samples


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

# 两行可用性时间轴（3 天、每 15 分钟一个点；黑=通、白=不通）
columns = [
    ("示例站点一", 98, [(57, 2), (131, 3)]),           # (不通起始点, 点数)
    ("示例站点二", 96, [(88, 2), (160, 5), (190, 1)]),
]
for i, (name, percent, outages) in enumerate(columns):
    y0 = 72 + i * 24
    d.text((px(10), px(y0)), name, font=font_cjk, fill=BLACK)
    text = f"{percent}%"
    tw = d.textlength(text, font=font_num) / SCALE
    d.text((px(W - 12 - tw), px(y0 + 4)), text, font=font_num, fill=BLACK)
    draw_timeline(build_timeline(outages), 10, y0 + 16, W - 20, 7)

if len(sys.argv) > 1:
    out_path = sys.argv[1]
else:
    out_dir = os.path.join(os.path.dirname(__file__), "..", "docs")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "preview.png")
img.save(out_path)
print("已生成预览图:", os.path.abspath(out_path))
