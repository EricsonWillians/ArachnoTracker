#!/usr/bin/env python3
"""Generate the ArachnoTracker logo: FastTracker-II-era DOS pixel aesthetic.

Chunky hand-drawn 5x7 bitmap glyphs scaled with crisp nearest-neighbor,
phosphor-green vertical gradient, glow, scanlines, and a pixel spider
(8 articulated legs, thorax + abdomen) hanging from a thread.
Output: assets/logo.png
"""
import os
from PIL import Image, ImageDraw, ImageFilter

GLYPHS = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "C": [".####", "#....", "#....", "#....", "#....", "#....", ".####"],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
}

SCALE = 10
GLYPH_W, GLYPH_H, GLYPH_GAP = 5, 7, 1

TITLE = "ARACHNOTRACKER"

# Spider pixel grid (19 wide x 22 tall), drawn algorithmically below:
# a thread, 8 jointed legs (roots on the thorax, knees up/out, tips down/out),
# a small cephalothorax and a larger abdomen.
SPIDER_W, SPIDER_H = 19, 22


def build_spider_grid():
    grid = [[0] * SPIDER_W for _ in range(SPIDER_H)]
    cx = SPIDER_W // 2  # 9

    def put(x, y):
        if 0 <= x < SPIDER_W and 0 <= y < SPIDER_H:
            grid[y][x] = 1

    def line(x0, y0, x1, y1):
        # Bresenham
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            put(x0, y0)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    # Thread.
    line(cx, 0, cx, 3)
    # Cephalothorax (small block) and abdomen (larger oval), vertically stacked
    # like a hanging spider.
    for y in range(4, 7):
        for x in range(cx - 1, cx + 2):
            put(x, y)
    for y in range(7, 13):
        half = 2 if y in (7, 12) else 3
        for x in range(cx - half, cx + half + 1):
            put(x, y)
    # Spinnerets hint.
    put(cx, 13)
    # Legs: 4 per side. (root, knee, tip) relative to center; knees raise the
    # leg before it drops to the tip — the classic spider silhouette.
    legs = [
        ((cx - 1, 4), (cx - 4, 1), (cx - 8, 3)),
        ((cx - 1, 5), (cx - 6, 3), (cx - 9, 7)),
        ((cx - 1, 6), (cx - 6, 8), (cx - 9, 12)),
        ((cx - 1, 7), (cx - 5, 11), (cx - 7, 16)),
    ]
    for root, knee, tip in legs:
        for mirror in (1, -1):
            r = (cx + (root[0] - cx) * mirror, root[1])
            k = (cx + (knee[0] - cx) * mirror, knee[1])
            t = (cx + (tip[0] - cx) * mirror, tip[1])
            line(r[0], r[1], k[0], k[1])
            line(k[0], k[1], t[0], t[1])
    return grid


SPIDER = build_spider_grid()


def text_width(text, scale):
    return len(text) * (GLYPH_W + GLYPH_GAP) * scale - GLYPH_GAP * scale


def draw_text(mask_draw, text, x, y, scale):
    cx = x
    for ch in text:
        glyph = GLYPHS.get(ch.upper(), GLYPHS[" "])
        for row, line in enumerate(glyph):
            for col, bit in enumerate(line):
                if bit == "#":
                    mask_draw.rectangle(
                        [cx + col * scale, y + row * scale,
                         cx + (col + 1) * scale - 1, y + (row + 1) * scale - 1],
                        fill=255)
        cx += (GLYPH_W + GLYPH_GAP) * scale


def draw_spider(mask_draw, x, y, scale):
    for row, line in enumerate(SPIDER):
        for col, bit in enumerate(line):
            if bit:
                mask_draw.rectangle(
                    [x + col * scale, y + row * scale,
                     x + (col + 1) * scale - 1, y + (row + 1) * scale - 1],
                    fill=255)


def main():
    title_scale = SCALE
    title_w = text_width(TITLE, title_scale)
    title_h = GLYPH_H * title_scale
    spider_scale = 6
    spider_w = SPIDER_W * spider_scale
    spider_h = SPIDER_H * spider_scale

    pad_x, pad_y = 56, 34
    gap_spider_text = 48
    content_w = spider_w + gap_spider_text + title_w
    content_h = max(spider_h, title_h)
    W = content_w + pad_x * 2
    H = content_h + pad_y * 2
    spider_x = pad_x
    spider_y = pad_y + (content_h - spider_h) // 2
    text_x = pad_x + spider_w + gap_spider_text
    title_y = pad_y + (content_h - title_h) // 2

    # Foreground mask (white shapes on black).
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    draw_spider(md, spider_x, spider_y, spider_scale)
    draw_text(md, TITLE, text_x, title_y, title_scale)

    # Phosphor vertical gradient.
    grad = Image.new("RGB", (1, H))
    top = (150, 255, 170)
    mid = (46, 220, 100)
    bot = (8, 110, 46)
    for y in range(H):
        t = y / max(1, H - 1)
        if t < 0.5:
            k = t / 0.5
            c = tuple(int(top[i] + (mid[i] - top[i]) * k) for i in range(3))
        else:
            k = (t - 0.5) / 0.5
            c = tuple(int(mid[i] + (bot[i] - mid[i]) * k) for i in range(3))
        grad.putpixel((0, y), c)
    grad = grad.resize((W, H))

    bg = Image.new("RGB", (W, H), (6, 10, 7))
    fg = Image.composite(grad, bg, mask)

    # Glow: blurred copy of the mask, dim green, behind the crisp pixels.
    glow_mask = mask.filter(ImageFilter.GaussianBlur(6))
    glow = Image.new("RGB", (W, H), (20, 160, 70))
    bg = Image.composite(glow, bg, glow_mask.point(lambda v: v * 55 // 100))
    out = Image.composite(fg, bg, mask)

    # Scanlines: darken every other pixel row (CRT feel).
    px = out.load()
    for y in range(0, H, 2):
        for x in range(W):
            r, g, b = px[x, y]
            px[x, y] = (r * 82 // 100, g * 82 // 100, b * 82 // 100)

    # Subtle border frame, tracker-panel style.
    frame = ImageDraw.Draw(out)
    frame.rectangle([6, 6, W - 7, H - 7], outline=(24, 90, 44), width=3)
    frame.rectangle([12, 12, W - 13, H - 13], outline=(12, 44, 22), width=1)

    dest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "logo.png")
    dest = os.path.normpath(dest)
    out.save(dest)
    print(f"wrote {dest} ({W}x{H})")


if __name__ == "__main__":
    main()
