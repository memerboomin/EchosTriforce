# Textures GX (GameCube) : décodage des formats I4…CMPR en RGBA 8 bits, en-têtes BTI, écriture PNG sans dépendance.
import struct
import zlib

I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8, C14X2, CMPR = 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 14
FORMAT_NAMES = {I4: "I4", I8: "I8", IA4: "IA4", IA8: "IA8", RGB565: "RGB565", RGB5A3: "RGB5A3", RGBA8: "RGBA8",
                C4: "C4", C8: "C8", C14X2: "C14X2", CMPR: "CMPR"}
# largeur, hauteur de bloc ; octets par bloc
BLOCK = {I4: (8, 8, 32), I8: (8, 4, 32), IA4: (8, 4, 32), IA8: (4, 4, 32), RGB565: (4, 4, 32), RGB5A3: (4, 4, 32),
         RGBA8: (4, 4, 64), C4: (8, 8, 32), C8: (8, 4, 32), C14X2: (4, 4, 32), CMPR: (8, 8, 32)}
TLUT_IA8, TLUT_RGB565, TLUT_RGB5A3 = 0, 1, 2


def _rgb565(v):
    r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2, 255)


def _rgb5a3(v):
    if v & 0x8000:
        r, g, b = (v >> 10) & 0x1F, (v >> 5) & 0x1F, v & 0x1F
        return (r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2, 255)
    a, r, g, b = (v >> 12) & 7, (v >> 8) & 0xF, (v >> 4) & 0xF, v & 0xF
    return (r * 17, g * 17, b * 17, a << 5 | a << 2 | a >> 1)


def _ia8(v):
    a, i = v >> 8, v & 0xFF
    return (i, i, i, a)


def data_size(fmt, w, h):
    bw, bh, bb = BLOCK[fmt]
    return ((w + bw - 1) // bw) * ((h + bh - 1) // bh) * bb


def decode_palette(data, off, fmt, count):
    conv = {TLUT_IA8: _ia8, TLUT_RGB565: _rgb565, TLUT_RGB5A3: _rgb5a3}[fmt]
    return [conv(v) for v in struct.unpack(">%dH" % count, data[off:off + count * 2])]


def decode(data, off, fmt, w, h, palette=None):
    """Renvoie un bytearray RGBA (w*h*4), lignes de haut en bas."""
    bw, bh, bb = BLOCK[fmt]
    out = bytearray(w * h * 4)
    bx_n = (w + bw - 1) // bw
    by_n = (h + bh - 1) // bh
    p = off

    def put(x, y, c):
        if x < w and y < h:
            i = (y * w + x) * 4
            out[i:i + 4] = bytes(c)

    for by in range(by_n):
        for bx in range(bx_n):
            x0, y0 = bx * bw, by * bh
            blk = data[p:p + bb]
            p += bb
            if fmt == CMPR:
                for sub in range(4):
                    s = blk[sub * 8:sub * 8 + 8]
                    c0, c1 = struct.unpack(">HH", s[0:4])
                    a, b = _rgb565(c0), _rgb565(c1)
                    if c0 > c1:
                        cols = [a, b, tuple((2 * a[k] + b[k]) // 3 for k in range(3)) + (255,),
                                tuple((a[k] + 2 * b[k]) // 3 for k in range(3)) + (255,)]
                    else:
                        cols = [a, b, tuple((a[k] + b[k]) // 2 for k in range(3)) + (255,), (0, 0, 0, 0)]
                    sx, sy = x0 + (sub & 1) * 4, y0 + (sub >> 1) * 4
                    for yy in range(4):
                        row = s[4 + yy]
                        for xx in range(4):
                            put(sx + xx, sy + yy, cols[(row >> (6 - 2 * xx)) & 3])
            elif fmt == RGBA8:
                for k in range(16):
                    ar = blk[k * 2:k * 2 + 2]
                    gb = blk[32 + k * 2:32 + k * 2 + 2]
                    put(x0 + (k & 3), y0 + (k >> 2), (ar[1], gb[0], gb[1], ar[0]))
            elif fmt in (I4, C4):
                for k in range(64):
                    v = blk[k >> 1]
                    v = (v >> 4) if (k & 1) == 0 else (v & 0xF)
                    if fmt == I4:
                        c = (v * 17,) * 3 + (v * 17,)
                    else:
                        c = palette[v] if palette and v < len(palette) else (0, 0, 0, 0)
                    put(x0 + (k & 7), y0 + (k >> 3), c)
            elif fmt in (I8, IA4, C8):
                for k in range(32):
                    v = blk[k]
                    if fmt == I8:
                        c = (v, v, v, v)
                    elif fmt == IA4:
                        i, a = (v & 0xF) * 17, (v >> 4) * 17
                        c = (i, i, i, a)
                    else:
                        c = palette[v] if palette and v < len(palette) else (0, 0, 0, 0)
                    put(x0 + (k & 7), y0 + (k >> 3), c)
            else:  # formats 16 bits en blocs 4x4
                vals = struct.unpack(">16H", blk)
                for k in range(16):
                    v = vals[k]
                    if fmt == IA8:
                        c = (v & 0xFF,) * 3 + (v >> 8,)
                    elif fmt == RGB565:
                        c = _rgb565(v)
                    elif fmt == RGB5A3:
                        c = _rgb5a3(v)
                    else:  # C14X2
                        idx = v & 0x3FFF
                        c = palette[idx] if palette and idx < len(palette) else (0, 0, 0, 0)
                    put(x0 + (k & 3), y0 + (k >> 2), c)
    return out


class Texture:
    """En-tête BTI (0x20 octets), commun aux .bti et aux entrées de TEX1."""

    def __init__(self, data, hdr_off, name=""):
        h = data[hdr_off:hdr_off + 0x20]
        (self.fmt, self.alpha, self.width, self.height, self.wrap_s, self.wrap_t, self.pal_on, self.pal_fmt,
         self.pal_count, pal_off, _b0, _b1, _b2, _b3, self.min_filter, self.mag_filter, _u, self.mip_count, _u2,
         _lod, img_off) = struct.unpack(">BBHHBBBBHI4BBBHBBhI", h)
        self.name = name
        self._data = data
        self._img = hdr_off + img_off
        self._pal = hdr_off + pal_off
        self._rgba = None

    @property
    def rgba(self):
        if self._rgba is None:
            pal = None
            if self.fmt in (C4, C8, C14X2):
                pal = decode_palette(self._data, self._pal, self.pal_fmt, self.pal_count)
            self._rgba = decode(self._data, self._img, self.fmt, self.width, self.height, pal)
        return self._rgba

    def has_alpha(self):
        px = self.rgba
        return any(px[i] < 250 for i in range(3, len(px), 4))

    def save_png(self, path):
        write_png(path, self.width, self.height, self.rgba)


def load_bti(data, name=""):
    return Texture(data, 0, name)


def write_png(path, w, h, rgba):
    stride = w * 4
    raw = b"".join(b"\0" + bytes(rgba[y * stride:(y + 1) * stride]) for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
