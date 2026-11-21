#!/bin/sh
# Compila o firmware do controle arcade num contêiner Debian 12.
#
# Uso (ou "make uf2" em firmware/arcade):
#   docker run --rm -e HOST_UID=$(id -u) -v "$PWD":/src:ro \
#     -v snespi-pico:/work -v "$PWD/firmware/arcade/build":/out \
#     debian:12 sh /src/scripts/firmware-container.sh
#
# /src  = este repositório (somente leitura)
# /work = pico-sdk e builds (persistente entre execuções)
# /out  = recebe os arquivos .uf2
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
	build-essential ca-certificates cmake git python3 \
	gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
	>/dev/null

uid="${HOST_UID:-1000}"
getent passwd builder >/dev/null || useradd -m -u "$uid" -o builder
mkdir -p /work /out
chown builder: /work /out

exec runuser -u builder -- env WORK=/work OUT=/out sh /src/scripts/build-firmware.sh
