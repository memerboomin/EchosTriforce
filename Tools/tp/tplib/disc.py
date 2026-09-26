# Image disque GameCube : en-tête, table des fichiers (FST), lecture d'un fichier par chemin.
import struct


class Disc:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        head = self._read(0, 0x440)
        self.game_id = head[0:6].decode("ascii")
        self.title = head[0x20:0x60].split(b"\0")[0].decode("latin-1")
        fst_off, fst_size = struct.unpack(">II", head[0x424:0x42C])
        fst = self._read(fst_off, fst_size)
        count = struct.unpack(">I", fst[8:12])[0]
        names = count * 12
        self.files = {}  # chemin -> (offset, taille)

        def name_at(o):
            end = fst.index(b"\0", names + o)
            return fst[names + o:end].decode("latin-1")

        # parcours récursif des répertoires : un répertoire s'étend jusqu'à l'entrée « next »
        def walk(first, end, prefix):
            i = first
            while i < end:
                e = fst[i * 12:(i + 1) * 12]
                is_dir = e[0] == 1
                name = name_at(int.from_bytes(e[1:4], "big"))
                a, b = struct.unpack(">II", e[4:12])
                if is_dir:
                    walk(i + 1, b, prefix + name + "/")
                    i = b
                else:
                    self.files[prefix + name] = (a, b)
                    i += 1

        walk(1, count, "")

    def _read(self, off, size):
        self.f.seek(off)
        return self.f.read(size)

    def read(self, path):
        key = self.find(path)
        off, size = self.files[key]
        return self._read(off, size)

    def find(self, path):
        """Chemin exact, sinon recherche insensible à la casse."""
        path = path.lstrip("/")
        if path in self.files:
            return path
        low = path.lower()
        for k in self.files:
            if k.lower() == low:
                return k
        raise KeyError(path)
