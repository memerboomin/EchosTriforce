#!/usr/bin/env python3
"""Planche contact des aperçus Blender : python3 Tools/art/contact_sheet.py <dossier> <préfixe> <sortie.png>"""
import sys, glob
from PIL import Image
d, prefix, dst = sys.argv[1:4]
files = sorted(glob.glob(f"{d}/{prefix}_*.png"), key=lambda p: int(p.rsplit("_", 1)[1][:-4]))
ims = [Image.open(f).convert("RGB").resize((500, 500)) for f in files]
cols = 4
W = Image.new("RGB", (500 * cols, 500 * ((len(ims) + cols - 1) // cols)), (200, 200, 200))
for i, im in enumerate(ims):
    W.paste(im, (500 * (i % cols), 500 * (i // cols)))
W.save(dst)
