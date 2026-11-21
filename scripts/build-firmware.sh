#!/bin/sh
# Compila o firmware do controle arcade (Raspberry Pi Pico) nas versões de
# 1 e 2 jogadores.
#
# Precisa de: cmake, make, git, python3, gcc/g++ do host e gcc-arm-none-eabi
# (com newlib). No Docker, use scripts/firmware-container.sh.
#
# Variáveis:
#   WORK  pasta de trabalho (pico-sdk e builds); padrão: firmware/arcade/build
#   OUT   onde ficam os .uf2 (snespi-arcade-<N>p-v<versão>.uf2); padrão: $WORK
set -eu

SDK_VERSION=2.3.1

src="$(cd "$(dirname "$0")/.." && pwd)/firmware/arcade"
WORK="${WORK:-$src/build}"
OUT="${OUT:-$WORK}"
sdk="$WORK/pico-sdk-$SDK_VERSION"
mkdir -p "$WORK" "$OUT"

if [ ! -f "$sdk/lib/tinyusb/src/tusb.h" ]; then
	rm -rf "$sdk"
	git clone -q --depth 1 --branch "$SDK_VERSION" \
		https://github.com/raspberrypi/pico-sdk.git "$sdk"
	git -C "$sdk" submodule update -q --init --depth 1 lib/tinyusb
fi

# Compila uma cópia dos fontes: o pico-sdk altera a data do CMakeLists.txt do
# projeto, e no contêiner o repositório é somente leitura.
rm -rf "$WORK/src"
mkdir -p "$WORK/src"
cp -R "$src/CMakeLists.txt" "$src/VERSION" "$src/src" "$WORK/src/"
version="$(cat "$src/VERSION")"
rm -f "$OUT"/snespi-arcade-*.uf2

for players in 1 2; do
	b="$WORK/build-${players}p"
	PICO_SDK_PATH="$sdk" cmake -S "$WORK/src" -B "$b" \
		-DCMAKE_BUILD_TYPE=Release -DARCADE_PLAYERS="$players" \
		-DPICOTOOL_FETCH_FROM_GIT_PATH="$WORK/picotool" >/dev/null
	make -C "$b" -j"$(nproc 2>/dev/null || echo 2)" >/dev/null
	cp "$b/snespi-arcade-${players}p.uf2" "$OUT/snespi-arcade-${players}p-v$version.uf2"
done
ls -l "$OUT"/snespi-arcade-*.uf2
