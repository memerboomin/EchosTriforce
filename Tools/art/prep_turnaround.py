#!/usr/bin/env python3
"""Prépare l'atlas de texture d'un personnage à partir de planches de face et de dos en pose T.
- détoure le fond blanc,
- étend les couleurs du personnage au-delà de sa silhouette (évite les liserés blancs aux jointures de projection),
- assemble face (moitié gauche) et dos (moitié droite) dans un atlas 4096 x 2048.
Usage : python3 Tools/art/prep_turnaround.py face.jpg dos.jpg sortie.png"""
import sys
from PIL import Image, ImageFilter, ImageChops

ATLAS_W, ATLAS_H = 4096, 2048
HALF = ATLAS_W // 2


def bleed(img, steps=48):
    """Remplit le fond blanc par dilatation successive des couleurs du personnage."""
    rgb = img.convert("RGB")
    px = rgb.load()
    w, h = rgb.size
    mask = Image.new("L", (w, h), 0)
    mp = mask.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if not (r > 232 and g > 232 and b > 232):
                mp[x, y] = 255
    mask = mask.filter(ImageFilter.MinFilter(3))  # rogne le liseré antialiasé blanc
    rgba = rgb.copy()
    rgba.putalpha(mask)
    cur = rgba
    for i in range(steps):
        grown = cur.filter(ImageFilter.MaxFilter(3))
        # ne garder que les pixels nouvellement couverts, en conservant la couleur déjà présente
        alpha = cur.getchannel("A")
        base = Image.composite(cur, grown, alpha)
        cur = base
        if i % 8 == 7:
            cur = cur.filter(ImageFilter.GaussianBlur(0.6))
            cur.putalpha(Image.eval(cur.getchannel("A"), lambda a: 255 if a > 8 else 0))
    out = Image.new("RGB", (w, h), (70, 90, 60))
    out.paste(cur.convert("RGB"), (0, 0), cur.getchannel("A"))
    # le personnage d'origine par-dessus, net
    out.paste(rgb, (0, 0), mask)
    return out


def main():
    front, back, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    f = bleed(Image.open(front))
    b = bleed(Image.open(back))
    scale = HALF / f.size[0]
    size = (HALF, int(round(f.size[1] * scale)))
    f = f.resize(size, Image.LANCZOS)
    b = b.resize(size, Image.LANCZOS)
    atlas = Image.new("RGB", (ATLAS_W, ATLAS_H), (70, 90, 60))
    # étirer la dernière ligne vers le bas pour remplir l'atlas proprement
    atlas.paste(f.resize((HALF, ATLAS_H), Image.NEAREST).crop((0, 0, HALF, ATLAS_H)), (0, 0))
    atlas.paste(b.resize((HALF, ATLAS_H), Image.NEAREST).crop((0, 0, HALF, ATLAS_H)), (HALF, 0))
    atlas.paste(f, (0, 0))
    atlas.paste(b, (HALF, 0))
    atlas.save(dst)
    print("atlas", dst, "échelle", scale, "hauteur utile", size[1])


if __name__ == "__main__":
    main()
