#!/bin/sh
# Teste de boot no QEMU (máquina raspi1ap, mesmo SoC BCM2835 do Pi 1).
#
# Boota a sdcard.img, espera o login aparecer na serial (init terminou),
# tira uma captura da tela (onde o launcher deve estar) e confere que ela
# tem conteúdo.
#
# NÃO testa: GPU VideoCore (o PiSNES não abre no QEMU), áudio, firmware
# (config.txt/start.elf são ignorados: o QEMU carrega o kernel direto).
#
# Uso: qemu-smoke.sh PASTA_DAS_IMAGENS PASTA_DE_SAIDA
# Precisa de: qemu-system-arm, qemu-img, python3
set -eu

IMAGES="$1"
OUT="$2"
TIMEOUT="${QEMU_BOOT_TIMEOUT:-300}"
HERE=$(cd "$(dirname "$0")" && pwd)

mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
LOG="$OUT/serial.log"
MON="$OUT/monitor.sock"
SD="$OUT/sdcard-qemu.img"

# O QEMU exige cartão SD com tamanho em potência de 2
cp "$IMAGES/sdcard.img" "$SD"
qemu-img resize -f raw "$SD" 1G >/dev/null

# Sem firmware: a linha de comando do kernel vem daqui, não do cmdline.txt
APPEND="root=/dev/mmcblk0p2 rootfstype=ext4 rootwait ro console=tty1 console=ttyAMA0,115200 consoleblank=0 loglevel=6"

rm -f "$LOG" "$MON"
qemu-system-arm -M raspi1ap -m 512 \
	-kernel "$IMAGES/zImage" -dtb "$IMAGES/bcm2708-rpi-b-plus.dtb" \
	-drive file="$SD",if=sd,format=raw \
	-append "$APPEND" \
	-serial file:"$LOG" -monitor unix:"$MON",server,nowait \
	-display none -no-reboot &
QEMU_PID=$!
trap 'kill $QEMU_PID 2>/dev/null || true' EXIT INT TERM

result=1
elapsed=0
while [ "$elapsed" -lt "$TIMEOUT" ]; do
	if ! kill -0 "$QEMU_PID" 2>/dev/null; then
		echo "FALHOU: o QEMU terminou antes do fim do boot"
		break
	fi
	if grep -q "Kernel panic" "$LOG" 2>/dev/null; then
		echo "FALHOU: kernel panic"
		break
	fi
	if grep -q "login:" "$LOG" 2>/dev/null; then
		echo "Boot completo em ~${elapsed}s (login na serial)"
		result=0
		break
	fi
	sleep 5
	elapsed=$((elapsed + 5))
done
[ "$elapsed" -ge "$TIMEOUT" ] && echo "FALHOU: sem login na serial em ${TIMEOUT}s"

if [ "$result" -eq 0 ]; then
	# tempo para o launcher abrir e desenhar
	sleep 20
	if python3 -I "$HERE/qemu_screendump.py" "$MON" "$OUT/tela.png"; then
		echo "Tela com conteúdo (launcher)"
	else
		echo "FALHOU: a tela está vazia (launcher não desenhou?)"
		result=1
	fi
fi

echo "---- partes relevantes do log serial ----"
grep -E "Linux version|Machine model|mmcblk0|EXT4-fs|vfat|FAT-fs|S99game|snespi|login:|panic|Oops|error" "$LOG" | head -60 || true
rm -f "$SD"
exit "$result"
