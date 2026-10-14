"""Генерирует res/app.ico: вышитый ромб на льняном поле (только стандартная библиотека Python)."""
import struct, zlib, os, sys

def png(size):
    bg = (236, 233, 222, 255)
    red = (163, 31, 40, 255)
    dark = (27, 33, 29, 255)
    cells = 13
    cell = size / cells
    rows = []
    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            cx, cy = int(x / cell), int(y / cell)
            d = abs(cx - 6) + abs(cy - 6)
            inner_x, inner_y = (x / cell) % 1, (y / cell) % 1
            stitch = 0.12 < inner_x < 0.88 and 0.12 < inner_y < 0.88
            r = 0.5 * size / 1.0
            corner = ((x - size / 2) ** 2 + (y - size / 2) ** 2) ** 0.5
            c = bg
            if d in (6, 3) and stitch:
                c = red
            elif d == 0 and stitch:
                c = dark
            elif d == 1 and (cx == 6 or cy == 6) and stitch:
                c = red
            # скруглённые углы
            m = size * 0.16
            for (ax, ay) in ((m, m), (size - m, m), (m, size - m), (size - m, size - m)):
                if (x < m or x > size - m) and (y < m or y > size - m):
                    if ((x - ax) ** 2 + (y - ay) ** 2) ** 0.5 > m:
                        c = (0, 0, 0, 0)
            row += bytes(c)
        rows.append(bytes(row))
    raw = b"".join(rows)
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")

sizes = [16, 24, 32, 48, 64, 256]
imgs = [png(s) for s in sizes]
out = struct.pack("<HHH", 0, 1, len(sizes))
offset = 6 + 16 * len(sizes)
for s, data in zip(sizes, imgs):
    out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
out += b"".join(imgs)
path = sys.argv[1] if len(sys.argv) > 1 else "res/app.ico"
open(path, "wb").write(out)
print("icon", path, len(out), "bytes")
