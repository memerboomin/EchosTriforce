# Archives RARC (.arc) : arborescence de fichiers, éventuellement compressée en Yaz0 (l'archive ou chaque fichier).
import struct

from . import yaz0


def parse(data):
    """Renvoie {chemin relatif: octets} (sans le nom du nœud racine)."""
    if yaz0.is_yaz0(data):
        data = yaz0.decompress(data)
    if data[:4] != b"RARC":
        raise ValueError("pas une archive RARC")
    data_off = struct.unpack(">I", data[0xC:0x10])[0] + 0x20
    n_nodes, node_off, n_entries, entry_off, _str_size, str_off = struct.unpack(">IIIIII", data[0x20:0x38])
    node_off += 0x20
    entry_off += 0x20
    str_off += 0x20

    def name(o):
        end = data.index(b"\0", str_off + o)
        return data[str_off + o:end].decode("latin-1")

    nodes = []
    for i in range(n_nodes):
        o = node_off + i * 0x10
        _typ, noff, _hash, nfiles, first = struct.unpack(">4sIHHI", data[o:o + 0x10])
        nodes.append((name(noff), nfiles, first))

    files = {}

    def walk(idx, prefix, depth):
        if depth > 32:
            return
        _n, nfiles, first = nodes[idx]
        for j in range(first, first + nfiles):
            o = entry_off + j * 0x14
            _fid, _h, tf, doff, dsize = struct.unpack(">HHIII", data[o:o + 0x10])
            flags, noff = tf >> 24, tf & 0xFFFFFF
            nm = name(noff)
            if flags & 0x02:
                if nm not in (".", ".."):
                    walk(doff, prefix + nm + "/", depth + 1)
            else:
                blob = data[data_off + doff:data_off + doff + dsize]
                if yaz0.is_yaz0(blob):
                    blob = yaz0.decompress(blob)
                files[prefix + nm] = blob

    if nodes:
        walk(0, "", 0)
    return files
