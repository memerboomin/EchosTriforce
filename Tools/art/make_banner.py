#!/usr/bin/env python3
"""Texture de bannière à glyphe d'eau (maquettes 04/05) : tissu bleu-vert, bordures dorées, goutte et vagues lumineuses.
RGB = couleur, A = masque d'émission (glyphe). UV : u de gauche à droite, v = 0 en bas (pointe du fanion).
Usage : python3 Tools/art/make_banner.py Tools/art/out/T_Banner_Water.png"""
import math, random, sys
from PIL import Image, ImageDraw, ImageFilter

W, H = 512, 1024
rnd = random.Random(4)


def main(dst):
    base = Image.new("RGB", (W, H))
    px = base.load()
    for y in range(H):
        for x in range(W):
            t = y / H
            weave = 0.04 * math.sin(x * 0.9) * math.sin(y * 0.7) + 0.03 * math.sin(x * 0.05 + y * 0.013)
            v = 0.85 + 0.25 * (1 - abs(x / W - 0.5) * 2) ** 0.6 + weave - 0.15 * t
            px[x, y] = (int(14 * v), int(62 * v), int(72 * v))
    noise = Image.effect_noise((W, H), 18).convert("L").filter(ImageFilter.GaussianBlur(1.2))
    base = Image.blend(base, Image.merge("RGB", (noise, noise, noise)), 0.06)
    d = ImageDraw.Draw(base)
    gold, gold_dark = (212, 168, 76), (140, 98, 38)
    # bordures : double filet doré qui suit la pointe du fanion (le bas de la texture = pointe)
    tip_h = int(H * 0.15)
    def outline(inset, width, col):
        pts = [(inset, inset), (W - inset, inset), (W - inset, H - tip_h - inset * 0.3), (W // 2, H - inset * 1.6), (inset, H - tip_h - inset * 0.3), (inset, inset)]
        d.line(pts, fill=col, width=width, joint="curve")
    outline(14, 10, gold_dark)
    outline(14, 6, gold)
    outline(34, 3, gold)
    # frise du haut
    for i in range(8):
        cx = W * (i + 0.5) / 8
        d.polygon([(cx, 60), (cx + 14, 78), (cx, 96), (cx - 14, 78)], outline=gold, fill=None, width=3)
    # masque d'émission (glyphe)
    mask = Image.new("L", (W, H), 0)
    m = ImageDraw.Draw(mask)
    cx, cy, r = W // 2, int(H * 0.36), 88
    drop = []
    for k in range(120):
        a = 2 * math.pi * k / 120
        x = math.sin(a)
        y = -math.cos(a)
        if y < 0:  # moitié haute : pointe
            x *= (1 + y) ** 1.2
            y *= 1.9
        drop.append((cx + x * r, cy + y * r * 0.95 + r * 0.3))
    m.polygon(drop, fill=255)
    for i, yy in enumerate((cy + r * 1.75, cy + r * 2.35, cy + r * 2.95)):
        wpts = [(cx - 140 + 280 * t / 60, yy + 18 * math.sin(t / 60 * 2 * math.pi * 1.5)) for t in range(61)]
        m.line(wpts, fill=255, width=16 - i * 3, joint="curve")
    glow = mask.filter(ImageFilter.GaussianBlur(10))
    # couleur du glyphe : cyan pâle, halo sur le tissu
    glyph_col = Image.new("RGB", (W, H), (150, 245, 235))
    halo_col = Image.new("RGB", (W, H), (40, 170, 170))
    base = Image.composite(halo_col, base, glow.point(lambda v: int(v * 0.55)))
    base = Image.composite(glyph_col, base, mask)
    # reflet clair dans la goutte
    hl = Image.new("L", (W, H), 0)
    ImageDraw.Draw(hl).ellipse((cx - 50, cy + 18, cx - 22, cy + 78), fill=255)
    base = Image.composite(Image.new("RGB", (W, H), (235, 255, 252)), base, hl.filter(ImageFilter.GaussianBlur(4)))
    alpha = Image.eval(Image.merge("L", (mask,)), lambda v: v)
    alpha = Image.composite(mask, glow.point(lambda v: int(v * 0.35)), mask)
    out = base.copy()
    out.putalpha(alpha)
    out.save(dst)
    print("bannière", dst)


if __name__ == "__main__":
    main(sys.argv[1])
