#!/usr/bin/env python3
"""Gera ROMs de TESTE (arquivos falsos, só com o cabeçalho do cartucho
preenchido; nenhum código de jogo), capas e saves para testar o launcher.

Uso: make-test-roms.py PASTA
"""
import os
import struct
import sys
import zlib

MB = 1024 * 1024

# nome, mapa (byte 0x?FD5), tipo (0x?FD6), hirom, tamanho, região, copiadora, capa, save
ROMS = [
    ("Astro Rally.sfc", 0x20, 0x00, False, 1 * MB, 1, False, True, True),
    ("Castelo de Névoa.smc", 0x21, 0x02, True, 2 * MB, 1, True, True, True),
    ("Dragão de Cobalto.sfc", 0x23, 0x35, False, 3 * MB, 2, False, False, False),
    ("Estrela Polar.sfc", 0x20, 0x03, False, 1 * MB, 0, False, False, False),
    ("Fúria Turbo.sfc", 0x20, 0x15, False, 1 * MB + MB // 2, 1, False, False, False),
    ("Guardiões do Vale.sfc", 0x31, 0x02, True, 4 * MB, 1, False, True, True),
    ("Ilha Perdida.smc", 0x20, 0x02, False, 2 * MB, 2, True, True, True),
    ("Jornada Lunar.sfc", 0x30, 0x00, False, 1 * MB, 1, False, False, False),
    ("Kart Lendário.sfc", 0x20, 0x05, False, MB // 2, 1, False, True, True),
    ("Labirinto Zero.sfc", 0x20, 0x00, False, 1 * MB, 0, False, False, False),
    ("Mina Profunda.sfc", 0x32, 0x43, True, 4 * MB, 0, False, True, True),
    ("Noite de Neon.smc", 0x21, 0x00, True, 3 * MB, 2, False, False, False),
]


def header(title, mapmode, romtype, size, region):
    h = bytearray(0x50)
    h[0x00:0x06] = b"01TEST"
    t = title.encode("ascii", "replace")[:21].ljust(21, b" ")
    h[0x10:0x25] = t
    h[0x25] = mapmode
    h[0x26] = romtype
    h[0x27] = max(8, (size // 1024).bit_length() - 1)
    h[0x28] = 3
    h[0x29] = region
    h[0x2A] = 0x33
    struct.pack_into("<HH", h, 0x2C, 0xEDCB, 0x1234)  # complemento + checksum = 0xFFFF
    struct.pack_into("<H", h, 0x4C, 0x8000)  # vetor de reset
    return bytes(h)


def ascii_title(name):
    base = os.path.splitext(name)[0].upper()
    table = str.maketrans("ÁÀÂÃÉÊÍÓÔÕÚÇ", "AAAAEEIOOOUC")
    return base.translate(table)


def png(path, w, h, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixel(x, y))
    def chunk(kind, data):
        c = struct.pack(">I", len(data)) + kind + data
        return c + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)


def cover_pixel(seed):
    sky = [(27, 36, 51), (40, 30, 60), (20, 44, 52)][seed % 3]

    def px(x, y):
        bx, by = x // 16, y // 16  # "pixels" grandes de 16x16
        if (bx * 7 + by * 13 + seed) % 23 == 0 and by < 12:
            return bytes((232, 230, 225))  # estrelas
        if 30 <= bx <= 33 and 4 <= by <= 7:
            return bytes((242, 169, 59))  # sol
        ridge = 18 + ((bx + seed) % 9) // 3 - (2 if (bx // 5) % 2 else 0)
        if by >= ridge + 4:
            return bytes((14, 17, 22))
        if by >= ridge:
            return bytes((44, 58, 79))
        return bytes(sky)

    return px


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    os.makedirs(os.path.join(out, "capas"), exist_ok=True)
    os.makedirs(os.path.join(out, ".snes96_snapshots"), exist_ok=True)
    for i, (name, mapmode, romtype, hirom, size, region, copier, cover, save) in enumerate(ROMS):
        path = os.path.join(out, name)
        base = 0xFFB0 if hirom else 0x7FB0
        with open(path, "wb") as f:
            if copier:
                f.write(b"\0" * 512)
            start = f.tell()
            f.seek(start + size - 1)
            f.write(b"\0")
            f.seek(start + base)
            f.write(header(ascii_title(name), mapmode, romtype, size, region))
        stem = os.path.splitext(name)[0]
        if cover:
            png(os.path.join(out, "capas", stem + ".png"), 640, 456, cover_pixel(i))
        if save:
            with open(os.path.join(out, ".snes96_snapshots", stem + ".srm"), "wb") as f:
                f.write(b"\0" * 8192)
    # favoritos e recentes (o que o launcher grava em /roms/.snespi)
    os.makedirs(os.path.join(out, ".snespi"), exist_ok=True)
    with open(os.path.join(out, ".snespi", "favoritos.txt"), "w") as f:
        f.write("Castelo de Névoa\nIlha Perdida\nKart Lendário\nROM que foi apagada\n")
    with open(os.path.join(out, ".snespi", "recentes.txt"), "w") as f:
        f.write("Ilha Perdida\nDragão de Cobalto\nCastelo de Névoa\n")
    # um arquivo que não é ROM, para garantir que é ignorado
    with open(os.path.join(out, "dir.txt"), "w") as f:
        f.write("teste\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
