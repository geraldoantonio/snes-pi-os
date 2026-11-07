#!/bin/bash
# Gera sdcard.img com 3 partições: boot (FAT), rootfs (ext4) e roms (FAT32).
# Baseado em board/raspberrypi/post-image.sh do Buildroot.

set -e

BOARD_DIR="$(dirname "$0")"
GENIMAGE_CFG="${BINARIES_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

# Arquivos da partição de boot: DTBs, firmware (inclui config.txt e
# cmdline.txt) e o kernel indicado no config.txt
FILES=()
for i in "${BINARIES_DIR}"/*.dtb "${BINARIES_DIR}"/rpi-firmware/*; do
	FILES+=( "${i#"${BINARIES_DIR}"/}" )
done
KERNEL=$(sed -n 's/^kernel=//p' "${BINARIES_DIR}/rpi-firmware/config.txt")
FILES+=( "${KERNEL}" )

BOOT_FILES=$(printf '\\t\\t\\t"%s",\\n' "${FILES[@]}")

# Conteúdo inicial da partição de ROMs: instruções (dir.txt, que o GUI do
# PiSNES ignora) e o que o pacote snespi-homebrew deixou em roms-extra/
# (jogos de teste e licença). O genimage copia cada um para a raiz.
cp "${BOARD_DIR}/dir.txt" "${BINARIES_DIR}/dir.txt"
RFILES=( "dir.txt" )
for i in "${BINARIES_DIR}"/roms-extra/*; do
	[ -f "$i" ] && RFILES+=( "${i#"${BINARIES_DIR}"/}" )
done
ROMS_FILES=$(printf '\\t\\t\\t"%s",\\n' "${RFILES[@]}")

sed -e "s|#BOOT_FILES#|${BOOT_FILES}|" -e "s|#ROMS_FILES#|${ROMS_FILES}|" \
	"${BOARD_DIR}/genimage.cfg.in" > "${GENIMAGE_CFG}"

trap 'rm -rf "${ROOTPATH_TMP}"' EXIT
ROOTPATH_TMP="$(mktemp -d)"

rm -rf "${GENIMAGE_TMP}"

genimage \
	--rootpath "${ROOTPATH_TMP}"   \
	--tmppath "${GENIMAGE_TMP}"    \
	--inputpath "${BINARIES_DIR}"  \
	--outputpath "${BINARIES_DIR}" \
	--config "${GENIMAGE_CFG}"
