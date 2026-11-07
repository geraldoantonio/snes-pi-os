#!/bin/sh
# Baixa o Buildroot na versão fixada e gera a imagem do snes-pi-os.
# Precisa de um Linux com as dependências do Buildroot (ver ci-container.sh).
set -eu

BR_VERSION=2025.02.19
BR_GIT=https://gitlab.com/buildroot.org/buildroot.git

SRC=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$SRC/../snespi-work}

BR_DIR="$WORK/buildroot-$BR_VERSION"
OUT="$WORK/output"
export BR2_DL_DIR="$WORK/dl"
export BR2_CCACHE_DIR="$WORK/ccache"

if [ ! -d "$BR_DIR" ]; then
	git clone -q --depth 1 --branch "$BR_VERSION" "$BR_GIT" "$BR_DIR"
fi

make -C "$BR_DIR" BR2_EXTERNAL="$SRC/external" O="$OUT" snespi_defconfig
# BR2_JLEVEL (opcional): tarefas em paralelo. Útil no Docker Desktop com
# pouca RAM (ex.: 4 GB), onde o padrão (núcleos + 1) pode estourar a memória.
make -C "$BR_DIR" O="$OUT" ${BR2_JLEVEL:+BR2_JLEVEL=$BR2_JLEVEL}

xz -T0 -f -k "$OUT/images/sdcard.img"
ls -lh "$OUT/images/sdcard.img" "$OUT/images/sdcard.img.xz"
