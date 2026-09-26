# Décompression Yaz0 (LZ de Nintendo).


def is_yaz0(data):
    return data[:4] == b"Yaz0"


def decompress(data):
    size = int.from_bytes(data[4:8], "big")
    out = bytearray(size)
    src, dst = 16, 0
    while dst < size:
        code = data[src]
        src += 1
        for _ in range(8):
            if dst >= size:
                break
            if code & 0x80:
                out[dst] = data[src]
                dst += 1
                src += 1
            else:
                b1, b2 = data[src], data[src + 1]
                src += 2
                dist = ((b1 & 0xF) << 8 | b2) + 1
                cnt = b1 >> 4
                if cnt == 0:
                    cnt = data[src] + 0x12
                    src += 1
                else:
                    cnt += 2
                cnt = min(cnt, size - dst)
                s = dst - dist
                if dist >= cnt:
                    out[dst:dst + cnt] = out[s:s + cnt]
                else:  # recouvrement : le motif de « dist » octets se répète
                    chunk = bytes(out[s:dst])
                    out[dst:dst + cnt] = (chunk * (cnt // dist + 1))[:cnt]
                dst += cnt
            code <<= 1
    return bytes(out)
