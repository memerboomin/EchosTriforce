# Modèles J3D (BMD/BDL) : squelette (JNT1/INF1), enveloppes (EVP1/DRW1), géométrie (VTX1/SHP1),
# matériaux simplifiés (MAT3) et textures (TEX1). La géométrie est ramenée en pose de repos, dans l'espace du modèle.
import math
import struct

from . import gxtex

# attributs GX
VA_PNMTXIDX, VA_POS, VA_NRM, VA_CLR0, VA_CLR1, VA_TEX0 = 0, 9, 10, 11, 12, 13
VA_NBT = 25
# types de composantes
U8, S8, U16, S16, F32 = 0, 1, 2, 3, 4
COMP_FMT = {U8: ("B", 1), S8: ("b", 1), U16: ("H", 2), S16: ("h", 2), F32: ("f", 4)}


# ---------------------------------------------------------------------------- matrices 3x4 / 4x4 (listes de lignes)
def mat_identity():
    return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def mat_srt(s, r, t):
    """T * Rz * Ry * Rx * S (convention J3D, angles en radians)."""
    sx, sy, sz = s
    cx, sx_ = math.cos(r[0]), math.sin(r[0])
    cy, sy_ = math.cos(r[1]), math.sin(r[1])
    cz, sz_ = math.cos(r[2]), math.sin(r[2])
    m = [
        [cy * cz, sx_ * sy_ * cz - cx * sz_, cx * sy_ * cz + sx_ * sz_, t[0]],
        [cy * sz_, sx_ * sy_ * sz_ + cx * cz, cx * sy_ * sz_ - sx_ * cz, t[1]],
        [-sy_, sx_ * cy, cx * cy, t[2]],
        [0.0, 0.0, 0.0, 1.0],
    ]
    for i in range(3):
        m[i][0] *= sx
        m[i][1] *= sy
        m[i][2] *= sz
    return m


def xform_point(m, v):
    return (m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2] + m[0][3],
            m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2] + m[1][3],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2] + m[2][3])


def xform_dir(m, v):
    x = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2]
    y = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2]
    z = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]
    n = math.sqrt(x * x + y * y + z * z) or 1.0
    return (x / n, y / n, z / n)


# ---------------------------------------------------------------------------- structures
class Joint:
    def __init__(self, name, kind, scale_comp, scale, rot, trans):
        self.name = name
        self.kind = kind
        self.scale_comp = scale_comp
        self.scale = scale
        self.rot = rot  # radians
        self.trans = trans
        self.parent = -1
        self.world = None  # matrice monde de repos


class Material:
    def __init__(self, name):
        self.name = name
        self.textures = []  # indices TEX1 (8 emplacements, -1 si vide)
        self.main_texture = -1  # texture du premier étage TEV
        self.main_uv = 0  # jeu d'UV qui l'échantillonne
        self.env_map = False  # UV générées depuis les normales (reflets)
        self.cull = 2  # 0 aucun, 1 avant, 2 arrière
        self.alpha_test = False
        self.blend = False
        self.mat_color = (255, 255, 255, 255)
        self.uses_vertex_color = False


class Shape:
    def __init__(self, index):
        self.index = index
        self.material = -1
        self.positions = []  # [(x, y, z)] espace modèle, pose de repos
        self.normals = []
        self.colors = []  # [(r, g, b, a)] ou vide
        self.uvs = [[] for _ in range(8)]
        self.weights = []  # [[(joint, poids), ...]] par sommet
        self.triangles = []  # [(i0, i1, i2)] indices de sommets


class Model:
    def __init__(self):
        self.joints = []
        self.materials = []
        self.textures = []
        self.shapes = []
        self.vertex_count = 0


# ---------------------------------------------------------------------------- lecture
def _names(data, off):
    count = struct.unpack(">H", data[off:off + 2])[0]
    out = []
    for i in range(count):
        _hsh, so = struct.unpack(">HH", data[off + 4 + i * 4:off + 8 + i * 4])
        end = data.index(b"\0", off + so)
        out.append(data[off + so:end].decode("latin-1"))
    return out


def load(data):
    magic = data[:8]
    if magic not in (b"J3D2bmd3", b"J3D2bdl4", b"J3D2bmd2"):
        raise ValueError("pas un modèle J3D : %r" % magic)
    nblocks = struct.unpack(">I", data[0xC:0x10])[0]
    blocks = {}
    o = 0x20
    for _ in range(nblocks):
        tag = data[o:o + 4].decode("ascii")
        size = struct.unpack(">I", data[o + 4:o + 8])[0]
        blocks[tag] = o
        o += size
    m = Model()
    _read_joints(data, blocks["JNT1"], m)
    hierarchy = _read_inf1(data, blocks["INF1"], m)
    _compute_world(m)
    if "TEX1" in blocks:
        _read_textures(data, blocks["TEX1"], m)
    if "MAT3" in blocks:
        _read_materials(data, blocks["MAT3"], m)
    drw = _read_drw1(data, blocks["DRW1"])
    evp = _read_evp1(data, blocks["EVP1"])
    vtx = _read_vtx1(data, blocks["VTX1"])
    _read_shapes(data, blocks["SHP1"], m, vtx, drw, evp)
    for shape_idx, mat_idx in hierarchy:
        if shape_idx < len(m.shapes):
            m.shapes[shape_idx].material = mat_idx
    return m


def _read_inf1(data, o, m):
    m.vertex_count = struct.unpack(">I", data[o + 0x10:o + 0x14])[0]
    p = o + struct.unpack(">I", data[o + 0x14:o + 0x18])[0]
    stack = [-1]
    last_joint = -1
    cur_mat = -1
    shape_mats = []
    while True:
        typ, idx = struct.unpack(">HH", data[p:p + 4])
        p += 4
        if typ == 0x00:
            break
        if typ == 0x01:
            stack.append(last_joint)
        elif typ == 0x02:
            stack.pop()
        elif typ == 0x10:
            m.joints[idx].parent = stack[-1]
            last_joint = idx
        elif typ == 0x11:
            cur_mat = idx
        elif typ == 0x12:
            shape_mats.append((idx, cur_mat))
    return shape_mats


def _read_joints(data, o, m):
    count, init_off, idx_off, name_off = struct.unpack(">H2xIII", data[o + 8:o + 0x18])
    names = _names(data, o + name_off)
    for i in range(count):
        ii = struct.unpack(">H", data[o + idx_off + i * 2:o + idx_off + i * 2 + 2])[0]
        q = o + init_off + ii * 0x40  # J3DJointInitData : 0x40 octets (boîte englobante comprise)
        kind, scomp = struct.unpack(">HB", data[q:q + 3])
        sx, sy, sz, rx, ry, rz, tx, ty, tz = struct.unpack(">3f3h2x3f", data[q + 4:q + 0x24])
        k = math.pi / 32768.0
        m.joints.append(Joint(names[i] if i < len(names) else "joint%d" % i, kind, scomp,
                              (sx, sy, sz), (rx * k, ry * k, rz * k), (tx, ty, tz)))


def _compute_world(m):
    done = [False] * len(m.joints)

    def world(i):
        j = m.joints[i]
        if not done[i]:
            local = mat_srt(j.scale, j.rot, j.trans)
            j.world = local if j.parent < 0 else mat_mul(world(j.parent), local)
            done[i] = True
        return j.world

    for i in range(len(m.joints)):
        world(i)


def _read_drw1(data, o):
    count, flag_off, idx_off = struct.unpack(">H2xII", data[o + 8:o + 0x14])
    flags = data[o + flag_off:o + flag_off + count]
    idx = struct.unpack(">%dH" % count, data[o + idx_off:o + idx_off + count * 2])
    return [(flags[i], idx[i]) for i in range(count)]


def _read_evp1(data, o):
    count, cnt_off, idx_off, w_off, inv_off = struct.unpack(">H2xIIII", data[o + 8:o + 0x1C])
    envs = []
    if count == 0:
        return {"envs": envs, "inv": {}}
    counts = data[o + cnt_off:o + cnt_off + count]
    pi = o + idx_off
    pw = o + w_off
    max_joint = 0
    for c in counts:
        js = struct.unpack(">%dH" % c, data[pi:pi + c * 2])
        ws = struct.unpack(">%df" % c, data[pw:pw + c * 4])
        pi += c * 2
        pw += c * 4
        envs.append(list(zip(js, ws)))
        max_joint = max([max_joint] + list(js))
    inv = {}
    for j in range(max_joint + 1):
        q = o + inv_off + j * 48
        v = struct.unpack(">12f", data[q:q + 48])
        inv[j] = [list(v[0:4]), list(v[4:8]), list(v[8:12]), [0.0, 0.0, 0.0, 1.0]]
    return {"envs": envs, "inv": inv}


def _read_vtx1(data, o):
    fmt_off = struct.unpack(">I", data[o + 8:o + 0xC])[0]
    offs = struct.unpack(">13I", data[o + 0xC:o + 0x40])
    block_end = o + struct.unpack(">I", data[o + 4:o + 8])[0]
    # emplacements : 0 pos, 1 nrm, 2 nbt, 3-4 couleurs, 5-12 UV
    slot_of_attr = {VA_POS: 0, VA_NRM: 1, VA_NBT: 2, VA_CLR0: 3, VA_CLR1: 4}
    for i in range(8):
        slot_of_attr[VA_TEX0 + i] = 5 + i
    fmts = {}
    p = o + fmt_off
    while True:
        attr, cnt, typ, frac = struct.unpack(">IIIB", data[p:p + 13])
        p += 16
        if attr == 0xFF:
            break
        fmts[attr] = (cnt, typ, frac)
    starts = sorted(set(x for x in offs if x))
    arrays = {}
    for attr, (cnt, typ, frac) in fmts.items():
        slot = slot_of_attr.get(attr)
        if slot is None or not offs[slot]:
            continue
        start = offs[slot]
        nxt = [s for s in starts if s > start]
        end = (o + nxt[0]) if nxt else block_end
        start += o
        if attr in (VA_CLR0, VA_CLR1):
            arrays[attr] = _read_colors(data, start, end, cnt, typ)
            continue
        if attr == VA_POS:
            ncomp = 3 if cnt == 1 else 2
        elif attr in (VA_NRM, VA_NBT):
            ncomp = 3 if (cnt == 0 and attr == VA_NRM) else 9
        else:
            ncomp = 2 if cnt == 1 else 1
        ch, size = COMP_FMT[typ]
        stride = ncomp * size
        n = (end - start) // stride
        vals = struct.unpack(">%d%s" % (n * ncomp, ch), data[start:start + n * stride])
        scale = 1.0 if typ == F32 else 1.0 / (1 << frac)
        arrays[attr] = [tuple(vals[i * ncomp + k] * scale for k in range(ncomp)) for i in range(n)]
    return {"fmts": fmts, "arrays": arrays}


def _read_colors(data, start, end, cnt, typ):
    out = []
    p = start
    if typ in (1, 2, 5):  # RGB8, RGBX8, RGBA8
        step = 3 if typ == 1 else 4
        while p + step <= end:
            c = data[p:p + step]
            out.append((c[0], c[1], c[2], c[3] if typ == 5 else 255))
            p += step
    elif typ == 0:  # RGB565
        while p + 2 <= end:
            out.append(gxtex._rgb565(struct.unpack(">H", data[p:p + 2])[0]))
            p += 2
    elif typ == 3:  # RGBA4
        while p + 2 <= end:
            v = struct.unpack(">H", data[p:p + 2])[0]
            out.append(tuple(((v >> s) & 0xF) * 17 for s in (12, 8, 4, 0)))
            p += 2
    elif typ == 4:  # RGBA6
        while p + 3 <= end:
            v = int.from_bytes(data[p:p + 3], "big")
            out.append(tuple((((v >> s) & 0x3F) << 2) | (((v >> s) & 0x3F) >> 4) for s in (18, 12, 6, 0)))
            p += 3
    return out


def _read_textures(data, o, m):
    count, hdr_off, name_off = struct.unpack(">H2xII", data[o + 8:o + 0x14])
    names = _names(data, o + name_off)
    for i in range(count):
        m.textures.append(gxtex.Texture(data, o + hdr_off + i * 0x20, names[i] if i < len(names) else "tex%d" % i))


def _u16(data, p):
    return struct.unpack(">H", data[p:p + 2])[0]


def _read_materials(data, o, m):
    count = _u16(data, o + 8)
    offs = struct.unpack(">30I", data[o + 0xC:o + 0xC + 120])
    (init_off, id_off, name_off, _ind, cull_off, matcol_off, _ccn, chan_off, _amb, _light, _tgn, texcoord_off,
     _tc2, _tm, _f44, texno_off, tevorder_off, _tc, _tkc, _tsn, _tsi, _tsm, _tsmt, _fog, alphacomp_off, blend_off,
     _zm, _zc, _dither, _nbt) = offs
    names = _names(data, o + name_off)
    for i in range(count):
        mid = _u16(data, o + id_off + i * 2)
        q = o + init_off + mid * 0x14C
        mat = Material(names[i] if i < len(names) else "mat%d" % i)
        cull_idx = data[q + 1]
        mat.cull = struct.unpack(">I", data[o + cull_off + cull_idx * 4:o + cull_off + cull_idx * 4 + 4])[0]
        mc = _u16(data, q + 0x08)
        if mc != 0xFFFF and matcol_off:
            mat.mat_color = tuple(data[o + matcol_off + mc * 4:o + matcol_off + mc * 4 + 4])
        ch = _u16(data, q + 0x0C)
        if ch != 0xFFFF and chan_off:
            # J3DColorChanInfo : activation de l'éclairage, source matériau (0 registre, 1 sommet)
            info = data[o + chan_off + ch * 8:o + chan_off + ch * 8 + 8]
            mat.uses_vertex_color = info[1] == 1
        for k in range(8):
            ti = _u16(data, q + 0x84 + k * 2)
            mat.textures.append(_u16(data, o + texno_off + ti * 2) if ti != 0xFFFF else -1)
        # étages TEV : texture et jeu de coordonnées de chaque emplacement
        slot_coord = {}
        for k in range(16):
            to = _u16(data, q + 0xBC + k * 2)
            if to == 0xFFFF:
                continue
            tex_coord, tex_map, _chan = data[o + tevorder_off + to * 4:o + tevorder_off + to * 4 + 3]
            if tex_map != 0xFF and tex_map < 8 and tex_map not in slot_coord:
                slot_coord[tex_map] = tex_coord
        # texture principale : la plus grande des textures du matériau (l'iris plutôt que son ombre ou son reflet)
        best = -1
        for k, ti in enumerate(mat.textures):
            if ti < 0 or ti >= len(m.textures):
                continue
            area = m.textures[ti].width * m.textures[ti].height
            if best < 0 or area > m.textures[mat.textures[best]].width * m.textures[mat.textures[best]].height:
                best = k
        if best >= 0:
            mat.main_texture = mat.textures[best]
            tex_coord = slot_coord.get(best, 0xFF)
            if tex_coord != 0xFF:
                tci = _u16(data, q + 0x28 + tex_coord * 2)
                if tci != 0xFFFF:
                    _gtyp, src, _mtx = data[o + texcoord_off + tci * 4:o + texcoord_off + tci * 4 + 3]
                    # GX_TG_POS 0, GX_TG_NRM 1, GX_TG_BINRM 2, GX_TG_TANGENT 3, GX_TG_TEX0..7 = 4..11
                    if 4 <= src <= 11:
                        mat.main_uv = src - 4
                    elif src in (1, 2, 3):
                        mat.env_map = True
        ac = _u16(data, q + 0x146)
        if ac != 0xFFFF:
            comp0, ref0, op, comp1, ref1 = data[o + alphacomp_off + ac * 8:o + alphacomp_off + ac * 8 + 5]
            # GX_ALWAYS = 7 ; opérateurs GX_AOP_AND 0, GX_AOP_OR 1
            always = (op == 0 and comp0 == 7 and comp1 == 7) or (op == 1 and (comp0 == 7 or comp1 == 7))
            mat.alpha_test = not always
        bl = _u16(data, q + 0x148)
        if bl != 0xFFFF:
            btype = data[o + blend_off + bl * 4]
            mat.blend = btype == 1
        m.materials.append(mat)


def _read_shapes(data, o, m, vtx, drw, evp):
    (count, init_off, idx_off, _name_off, desc_off, mtxtab_off, dl_off, mtxinit_off,
     drawinit_off) = struct.unpack(">H2xIIIIIIII", data[o + 8:o + 0x2C])
    arrays = vtx["arrays"]
    joints = m.joints

    # matrice de chaque entrée DRW1 : (liste [(joint, poids)], matrice de liaison -> repos)
    drw_cache = {}

    def drw_info(di):
        if di in drw_cache:
            return drw_cache[di]
        weighted, idx = drw[di]
        if not weighted:
            res = ([(idx, 1.0)], joints[idx].world)
        else:
            env = evp["envs"][idx]
            acc = [[0.0] * 4 for _ in range(4)]
            for j, w in env:
                mm = mat_mul(joints[j].world, evp["inv"][j])
                for r in range(3):
                    for c in range(4):
                        acc[r][c] += mm[r][c] * w
            acc[3] = [0.0, 0.0, 0.0, 1.0]
            res = (env, acc)
        drw_cache[di] = res
        return res

    slots = [0] * 10
    for si in range(count):
        ii = _u16(data, o + idx_off + si * 2)
        q = o + init_off + ii * 0x28
        mtx_type = data[q]
        groups, desc_idx, mtxinit_idx, drawinit_idx = struct.unpack(">HHHH", data[q + 2:q + 0xA])
        # description des sommets de cette forme
        desc = []
        p = o + desc_off + desc_idx
        while True:
            attr, typ = struct.unpack(">II", data[p:p + 8])
            p += 8
            if attr == 0xFF:
                break
            desc.append((attr, typ))
        shape = Shape(si)
        vmap = {}
        for g in range(groups):
            gm = o + mtxinit_off + (mtxinit_idx + g) * 8
            use_idx, use_cnt, first = struct.unpack(">HHI", data[gm:gm + 8])
            table = [_u16(data, o + mtxtab_off + (first + k) * 2) for k in range(use_cnt)]
            if mtx_type == 3:
                for k, v in enumerate(table):
                    if v != 0xFFFF:
                        slots[k] = v
            else:
                slots[0] = use_idx
            gd = o + drawinit_off + (drawinit_idx + g) * 8
            dl_size, dl_start = struct.unpack(">II", data[gd:gd + 8])
            _read_display_list(data, o + dl_off + dl_start, dl_size, desc, arrays, slots, drw_info, shape, vmap)
        m.shapes.append(shape)


def _read_display_list(data, p, size, desc, arrays, slots, drw_info, shape, vmap):
    end = p + size
    while p < end:
        op = data[p]
        p += 1
        if op == 0:
            continue
        prim = op & 0xF8
        if prim not in (0x80, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8):
            break
        n = _u16(data, p)
        p += 2
        verts = []
        for _ in range(n):
            key = []
            for attr, typ in desc:
                if typ == 1:  # direct (index de matrice)
                    key.append(data[p])
                    p += 1
                elif typ == 2:
                    key.append(data[p])
                    p += 1
                elif typ == 3:
                    key.append(_u16(data, p))
                    p += 2
                else:
                    key.append(None)
            verts.append(_vertex(tuple(key), desc, arrays, slots, drw_info, shape, vmap))
        if prim == 0x90:
            for i in range(0, n - 2, 3):
                shape.triangles.append((verts[i], verts[i + 1], verts[i + 2]))
        elif prim == 0x98:
            for i in range(n - 2):
                if i & 1:
                    shape.triangles.append((verts[i + 1], verts[i], verts[i + 2]))
                else:
                    shape.triangles.append((verts[i], verts[i + 1], verts[i + 2]))
        elif prim == 0xA0:
            for i in range(1, n - 1):
                shape.triangles.append((verts[0], verts[i], verts[i + 1]))
        elif prim == 0x80:
            for i in range(0, n - 3, 4):
                shape.triangles.append((verts[i], verts[i + 1], verts[i + 2]))
                shape.triangles.append((verts[i], verts[i + 2], verts[i + 3]))


def _vertex(key, desc, arrays, slots, drw_info, shape, vmap):
    vals = dict(zip((a for a, _ in desc), key))
    mslot = vals.get(VA_PNMTXIDX)
    di = slots[mslot // 3] if mslot is not None else slots[0]
    full_key = (di,) + key
    if full_key in vmap:
        return vmap[full_key]
    weights, mtx = drw_info(di)
    pos = arrays[VA_POS][vals[VA_POS]]
    shape.positions.append(xform_point(mtx, pos))
    na = VA_NRM if VA_NRM in vals else VA_NBT
    if na in vals and na in arrays:
        shape.normals.append(xform_dir(mtx, arrays[na][vals[na]][:3]))
    if VA_CLR0 in vals and VA_CLR0 in arrays:
        shape.colors.append(arrays[VA_CLR0][vals[VA_CLR0]])
    for t in range(8):
        a = VA_TEX0 + t
        if a in vals and a in arrays:
            uv = arrays[a][vals[a]]
            shape.uvs[t].append((uv[0], uv[1] if len(uv) > 1 else 0.0))
    shape.weights.append(weights)
    vi = len(shape.positions) - 1
    vmap[full_key] = vi
    return vi
