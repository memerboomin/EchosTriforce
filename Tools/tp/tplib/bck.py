# Animations de squelette J3D (BCK, bloc ANK1) : lecture et échantillonnage (interpolation d'Hermite de JSystem).
import math
import struct


class Track:
    __slots__ = ("count", "offset", "tangent", "data", "is_rot")

    def __init__(self, count, offset, tangent, data, is_rot):
        self.count, self.offset, self.tangent, self.data, self.is_rot = count, offset, tangent, data, is_rot

    def sample(self, frame, default):
        n = self.count
        if n == 0:
            return default
        d, o = self.data, self.offset
        if n == 1:
            return float(d[o])
        stride = 3 if self.tangent == 0 else 4
        if frame < d[o]:
            return float(d[o + 1])
        last = o + (n - 1) * stride
        if frame >= d[last]:
            return float(d[last + 1])
        lo, hi = 0, n - 1  # recherche du segment [k, k+1] contenant frame
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if frame >= d[o + mid * stride]:
                lo = mid
            else:
                hi = mid
        k0 = o + lo * stride
        k1 = k0 + stride
        t0, v0 = d[k0], d[k0 + 1]
        t1, v1 = d[k1], d[k1 + 1]
        out0 = d[k0 + 2] if stride == 3 else d[k0 + 3]
        in1 = d[k1 + 2]
        return _hermite(frame, t0, v0, out0, t1, v1, in1)


def _hermite(f, t0, v0, s0, t1, v1, s1):
    # JMAHermiteInterpolation (JMath.h), tangentes exprimées par image
    a = f - t0
    b = a / (t1 - t0)
    c = b - 1.0
    d = (3.0 - 2.0 * b) * b * b
    return (1.0 - d) * v0 + d * v1 + (c * c * a) * s0 + (c * a * b) * s1


class Animation:
    def __init__(self, data, name=""):
        if data[:8] != b"J3D1bck1":
            raise ValueError("pas une animation BCK : %r" % data[:8])
        o = 0x20
        if data[o:o + 4] != b"ANK1":
            raise ValueError("bloc ANK1 absent")
        (self.loop_mode, self.rot_shift, self.frames, self.joint_count, s_n, r_n, t_n, tab_off, s_off, r_off,
         t_off) = struct.unpack(">BBhHHHHIIII", data[o + 8:o + 0x24])
        self.name = name
        scale = struct.unpack(">%df" % s_n, data[o + s_off:o + s_off + s_n * 4])
        rot = struct.unpack(">%dh" % r_n, data[o + r_off:o + r_off + r_n * 2])
        trans = struct.unpack(">%df" % t_n, data[o + t_off:o + t_off + t_n * 4])
        self.tracks = []  # par os : [[sx, rx, tx], [sy, ry, ty], [sz, rz, tz]]
        p = o + tab_off
        for _ in range(self.joint_count):
            axes = []
            for _axis in range(3):
                trio = []
                for kind, arr in ((0, scale), (1, rot), (2, trans)):
                    cnt, off, tan = struct.unpack(">HHH", data[p:p + 6])
                    p += 6
                    trio.append(Track(cnt, off, tan, arr, kind == 1))
                axes.append(trio)
            self.tracks.append(axes)
        self._rot_k = (1 << self.rot_shift) * math.pi / 32768.0

    def sample(self, joint, frame):
        """(échelle, rotation en radians, translation) locales de l'os à l'image donnée."""
        ax = self.tracks[joint]
        s = tuple(ax[i][0].sample(frame, 1.0) for i in range(3))
        r = []
        for i in range(3):
            tr = ax[i][1]
            if tr.count == 0:
                r.append(0.0)
            else:
                v = tr.sample(frame, 0.0)
                r.append(float(int(v)) * self._rot_k if tr.count > 1 else v * self._rot_k)
        t = tuple(ax[i][2].sample(frame, 0.0) for i in range(3))
        return s, tuple(r), t
