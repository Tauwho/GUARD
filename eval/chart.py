"""Small deterministic PNG heatmap using only the approved Python standard library."""

import struct
import zlib

FONT = {
    "L": (4, 4, 4, 4, 7),
    "0": (7, 5, 5, 5, 7),
    "1": (2, 6, 2, 2, 7),
    "2": (7, 1, 7, 4, 7),
    "3": (7, 1, 7, 1, 7),
    "4": (5, 5, 7, 1, 1),
    "5": (7, 4, 7, 1, 7),
    "6": (7, 4, 7, 5, 7),
    "7": (7, 1, 1, 1, 1),
    "8": (7, 5, 7, 5, 7),
    "A": (2, 5, 7, 5, 5),
    "B": (6, 5, 6, 5, 6),
    "C": (7, 4, 4, 4, 7),
    "E": (7, 4, 6, 4, 7),
    "F": (7, 4, 6, 4, 4),
    "H": (5, 5, 7, 5, 5),
    "I": (7, 2, 2, 2, 7),
    "M": (5, 7, 7, 5, 5),
    "N": (5, 7, 7, 7, 5),
    "O": (7, 5, 5, 5, 7),
    "P": (6, 5, 6, 4, 4),
    "R": (6, 5, 6, 5, 5),
    "S": (7, 4, 7, 1, 7),
    "T": (7, 2, 2, 2, 2),
    "X": (5, 5, 2, 5, 5),
    "-": (0, 0, 7, 0, 0),
    " ": (0, 0, 0, 0, 0),
}


def heatmap(path, rows, columns, cells):
    width, height = 700, 510
    pixels = bytearray([255] * width * height * 3)

    def rectangle(x, y, w, h, color):
        for yy in range(y, min(y + h, height)):
            start = (yy * width + x) * 3
            pixels[start : start + w * 3] = bytes(color) * w

    def label(x, y, text, scale=2):
        for character in text:
            for yy, bits in enumerate(FONT[character]):
                for xx in range(3):
                    if bits & (1 << (2 - xx)):
                        rectangle(x + xx * scale, y + yy * scale, scale, scale, (30, 40, 50))
            x += 4 * scale

    label(30, 18, "HOST MATRIX - REFERENCE MISMATCHES")
    for column, text in enumerate(columns):
        label(90 + column * 85, 58, text)
    colors = [(213, 239, 224), (255, 218, 147), (244, 163, 163), (220, 220, 220)]
    for row, text in enumerate(rows):
        y = 85 + row * 42
        label(28, y + 12, text)
        for column in range(len(columns)):
            value = cells.get((row, column), 3)
            rectangle(82 + column * 85, y, 72, 32, colors[value])
    for i, text in enumerate(("MATCH", "ABLATION", "MISMATCH", "ERROR")):
        rectangle(30 + i * 170, 480, 14, 14, colors[i])
        label(50 + i * 170, 482, text)

    def chunk(kind, data):
        return (
            struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        )

    raw = b"".join(b"\0" + pixels[y * width * 3 : (y + 1) * width * 3] for y in range(height))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )
