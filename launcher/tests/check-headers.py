#!/usr/bin/env python3
"""Confere o que o launcher lê das ROMs de teste (saída de --list).

Uso: check-headers.py BINARIO PASTA_DE_ROMS
"""
import os
import subprocess
import sys

MB = 1024 * 1024

# nome: (mapa, chip, suportado, região, save, tamanho)
EXPECTED = {
    "Astro Rally": ("LoROM", "-", 1, "NTSC", 1, 1 * MB),
    "Castelo de Névoa": ("HiROM", "-", 1, "NTSC", 1, 2 * MB),
    "Dragão de Cobalto": ("LoROM", "SA-1", 0, "PAL", 0, 3 * MB),
    "Estrela Polar": ("LoROM", "DSP-1", 1, "NTSC", 0, 1 * MB),
    "Fúria Turbo": ("LoROM", "Super FX", 0, "NTSC", 0, 1 * MB + MB // 2),
    "Guardiões do Vale": ("HiROM", "-", 1, "NTSC", 1, 4 * MB),
    "Ilha Perdida": ("LoROM", "-", 1, "PAL", 1, 2 * MB),
    "Jornada Lunar": ("LoROM", "-", 1, "NTSC", 0, 1 * MB),
    "Kart Lendário": ("LoROM", "DSP-1", 1, "NTSC", 1, MB // 2),
    "Labirinto Zero": ("LoROM", "-", 1, "NTSC", 0, 1 * MB),
    "Mina Profunda": ("HiROM", "SDD-1", 0, "NTSC", 1, 4 * MB),
    "Noite de Neon": ("HiROM", "-", 1, "PAL", 0, 3 * MB),
}


def main():
    binary, roms = sys.argv[1], sys.argv[2]
    env = dict(os.environ, HOME=roms)
    out = subprocess.run([binary, "--roms", roms, "--fonts", "fonts", "--list"],
                         capture_output=True, text=True, env=env, check=True).stdout
    got = {}
    order = []
    for line in out.splitlines():
        stem, mapa, chip, ok, reg, save, size = line.split("\t")
        got[stem] = (mapa, chip, int(ok), reg, int(save), int(size))
        order.append(stem)

    fails = 0
    for stem, exp in EXPECTED.items():
        if got.get(stem) != exp:
            print(f"FALHOU {stem}: esperado {exp}, obtido {got.get(stem)}")
            fails += 1
    extra = set(got) - set(EXPECTED)
    if extra:
        print(f"FALHOU: arquivos inesperados na lista: {sorted(extra)}")
        fails += 1
    if order != sorted(EXPECTED, key=lambda s: s.lower()):
        print(f"FALHOU: ordem {order}")
        fails += 1
    print("ok" if not fails else f"{fails} falha(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
