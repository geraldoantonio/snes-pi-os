#!/bin/sh
# Ajustes no rootfs depois dos pacotes e do overlay.
# Não chama o post-build.sh do raspberrypi: ele põe um getty (login) no
# tty1, que é a tela onde roda o jogo.

set -eu

TARGET_DIR="$1"
INITTAB="${TARGET_DIR}/etc/inittab"

# Ponto de montagem da partição de ROMs (a raiz é somente leitura)
mkdir -p "${TARGET_DIR}/roms"

# Frontend no tty1, reiniciado pelo init se sair ou cair. O BusyBox init
# só inicia entradas respawn depois de todos os sysinit (rcS), então a
# posição no arquivo não importa.
if ! grep -q 'snespi-frontend' "${INITTAB}"; then
	printf '\n# snes-pi-os: frontend na tela (HDMI/composto)\n%s\n' \
		'tty1::respawn:/usr/bin/snespi-frontend' >> "${INITTAB}"
fi
