#!/usr/bin/env python3
"""Génère les textures procédurales tuilables du projet (Tools/art/out/*.png).
Pierre de ruines, normales, bruit, eau, cascade. Aucune ressource externe : droits maîtrisés (chapitre 21)."""
import math, os, random
from PIL import Image, ImageFilter, ImageChops, ImageOps

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)
N = 1024
random.seed(7)


def value_noise(size, cell, seed):
    rnd = random.Random(seed)
    g = [[rnd.random() for _ in range(size // cell + 1)] for _ in range(size // cell + 1)]
    gw = size // cell
    img = Image.new("L", (size, size))
    px = img.load()
    for y in range(size):
        fy = y / cell
        y0 = int(fy) % gw
        y1 = (y0 + 1) % gw
        ty = fy - int(fy)
        ty = ty * ty * (3 - 2 * ty)
        for x in range(size):
            fx = x / cell
            x0 = int(fx) % gw
            x1 = (x0 + 1) % gw
            tx = fx - int(fx)
            tx = tx * tx * (3 - 2 * tx)
            a = g[y0][x0] * (1 - tx) + g[y0][x1] * tx
            b = g[y1][x0] * (1 - tx) + g[y1][x1] * tx
            px[x, y] = int((a * (1 - ty) + b * ty) * 255)
    return img


def fbm(size, seed, octaves=(128, 64, 32, 16, 8)):
    acc = [0.0] * (size * size)
    total = 0.0
    amp = 1.0
    for i, c in enumerate(octaves):
        data = list(value_noise(size, c, seed + i * 17).getdata())
        for k in range(len(acc)):
            acc[k] += data[k] * amp
        total += amp
        amp *= 0.55
    img = Image.new("L", (size, size))
    img.putdata([int(v / total) for v in acc])
    return img


def stone_bricks():
    """Grands blocs irréguliers façon ruines (albédo en niveaux de gris + hauteur)."""
    height = Image.new("L", (N, N), 0)
    albedo = Image.new("L", (N, N), 0)
    hp = height.load()
    ap = albedo.load()
    rows = 6
    rh = N // rows
    rnd = random.Random(3)
    base_noise = fbm(N, 11).load()
    fine = fbm(N, 29, (32, 16, 8, 4)).load()
    for r in range(rows):
        y0 = r * rh
        x = rnd.randint(0, 120) if r % 2 else 0
        widths = []
        total = 0
        while total < N:
            w = rnd.randint(150, 330)
            widths.append(w)
            total += w
        widths[-1] -= total - N
        cx = (rnd.randint(0, N)) if r % 2 else 0
        for w in widths:
            tone = rnd.uniform(0.72, 1.0)
            for yy in range(y0, y0 + rh):
                for xx in range(cx, cx + w):
                    X = xx % N
                    ex = min(xx - cx, cx + w - 1 - xx)
                    ey = min(yy - y0, y0 + rh - 1 - yy)
                    e = min(ex, ey)
                    # joint de mortier + chanfrein
                    if e < 4:
                        h = 0.15 + e * 0.05
                        a = 0.45
                    else:
                        bevel = min(1.0, (e - 4) / 14.0)
                        h = 0.5 + 0.5 * bevel
                        a = tone * (0.82 + 0.18 * bevel)
                    n = base_noise[X, yy] / 255.0
                    f = fine[X, yy] / 255.0
                    hp[X, yy] = int(max(0, min(1, h * (0.85 + 0.25 * n) - 0.1 * f)) * 255)
                    ap[X, yy] = int(max(0, min(1, a * (0.78 + 0.3 * n) * (0.9 + 0.15 * f))) * 255)
            cx += w
    return albedo, height


def normal_from_height(h, strength=3.0):
    w, hgt = h.size
    src = h.load()
    out = Image.new("RGB", (w, hgt))
    op = out.load()
    for y in range(hgt):
        for x in range(w):
            l = src[(x - 1) % w, y] / 255.0
            r = src[(x + 1) % w, y] / 255.0
            u = src[x, (y - 1) % hgt] / 255.0
            d = src[x, (y + 1) % hgt] / 255.0
            dx = (l - r) * strength
            dy = (u - d) * strength
            dz = 1.0
            ln = math.sqrt(dx * dx + dy * dy + dz * dz)
            op[x, y] = (int((dx / ln * 0.5 + 0.5) * 255), int((dy / ln * 0.5 + 0.5) * 255), int((dz / ln * 0.5 + 0.5) * 255))
    return out


def water_normal():
    size = 512
    img = Image.new("RGB", (size, size))
    px = img.load()
    waves = [(3, 1, 0.9), (1, 4, 0.7), (5, -2, 0.5), (-4, 3, 0.4), (7, 5, 0.25), (-9, 2, 0.2)]
    for y in range(size):
        for x in range(size):
            dx = dy = 0.0
            for kx, ky, a in waves:
                ph = 2 * math.pi * (kx * x + ky * y) / size
                c = math.cos(ph)
                dx += a * kx * c
                dy += a * ky * c
            dx *= 0.035
            dy *= 0.035
            ln = math.sqrt(dx * dx + dy * dy + 1)
            px[x, y] = (int((-dx / ln * 0.5 + 0.5) * 255), int((-dy / ln * 0.5 + 0.5) * 255), int((1 / ln * 0.5 + 0.5) * 255))
    return img


def waterfall_streaks():
    size = 256
    img = Image.new("L", (size, size))
    px = img.load()
    rnd = random.Random(5)
    cols = [rnd.random() for _ in range(size)]
    for x in range(size):
        cols[x] = (cols[x] + cols[(x + 1) % size] + cols[(x - 1) % size]) / 3
    n = value_noise(size, 16, 91).load()
    for y in range(size):
        for x in range(size):
            v = 0.35 + 0.65 * cols[x] * (0.6 + 0.4 * n[x, y] / 255.0)
            px[x, y] = int(max(0, min(1, v)) * 255)
    return img.filter(ImageFilter.GaussianBlur(1))


def main():
    print("pierre…")
    alb, h = stone_bricks()
    alb.save(os.path.join(OUT, "T_Stone_A.png"))
    h.save(os.path.join(OUT, "T_Stone_H.png"))
    print("normales…")
    normal_from_height(h, 4.0).save(os.path.join(OUT, "T_Stone_N.png"))
    print("bruit…")
    fbm(512, 101, (64, 32, 16, 8)).save(os.path.join(OUT, "T_Noise.png"))
    print("eau…")
    water_normal().save(os.path.join(OUT, "T_Water_N.png"))
    waterfall_streaks().save(os.path.join(OUT, "T_Fall.png"))
    print("ok", os.listdir(OUT))


if __name__ == "__main__":
    main()
