#!/usr/bin/env python3
"""Pede ao monitor do QEMU (HMP, socket unix) uma captura da tela, converte
de PPM para PNG e diz se a tela tem conteúdo (não está toda preta).

Uso: qemu_screendump.py SOCKET SAIDA.png
Sai com 0 se a captura tem conteúdo, 1 se está vazia, 2 em erro.
"""
import os
import socket
import struct
import sys
import time
import zlib


def hmp(sock_path, command, wait=3.0):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(wait)
    s.connect(sock_path)
    buf = b""
    try:
        while b"(qemu)" not in buf:
            buf += s.recv(4096)
        s.sendall(command.encode() + b"\n")
        buf = b""
        while buf.count(b"(qemu)") < 1:
            buf += s.recv(4096)
    except socket.timeout:
        pass
    finally:
        s.close()
    return buf.decode(errors="replace")


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos) + 1
            continue
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    if fields[0] != b"P6":
        raise ValueError("PPM inesperado: %r" % fields[0])
    w, h = int(fields[1]), int(fields[2])
    return w, h, data[pos + 1:pos + 1 + w * h * 3]


def write_png(path, w, h, rgb):
    raw = b"".join(b"\0" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind, payload):
        c = struct.pack(">I", len(payload)) + kind + payload
        return c + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 6)))
        f.write(chunk(b"IEND", b""))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    sock_path, out_png = sys.argv[1], sys.argv[2]
    ppm = out_png[:-4] + ".ppm" if out_png.endswith(".png") else out_png + ".ppm"
    if os.path.exists(ppm):
        os.remove(ppm)
    hmp(sock_path, "screendump " + os.path.abspath(ppm))
    for _ in range(20):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 20:
            break
        time.sleep(0.25)
    else:
        print("screendump: o QEMU não gerou a captura")
        return 2
    w, h, rgb = read_ppm(ppm)
    write_png(out_png, w, h, rgb)
    os.remove(ppm)

    # "Tem conteúdo" = pelo menos 1% dos pixels acima de um cinza bem escuro
    step = 3 * 7
    lit = sum(1 for i in range(0, len(rgb) - 2, step) if max(rgb[i:i + 3]) > 48)
    total = len(rgb) // step
    ratio = lit / total if total else 0
    print(f"captura {w}x{h}: {ratio:.1%} dos pixels com conteúdo -> {out_png}")
    return 0 if ratio >= 0.01 else 1


if __name__ == "__main__":
    sys.exit(main())
