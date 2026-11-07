#!/bin/sh
# Prepara um contêiner Debian 12 e roda o build como usuário comum.
#
# Uso (no CI ou localmente):
#   docker run --rm -e HOST_UID=$(id -u) \
#     -v "$PWD":/src:ro -v snespi-work:/work debian:12 /src/scripts/ci-container.sh
#
# /src  = este repositório (somente leitura)
# /work = Buildroot, downloads, ccache e output (persistente entre builds)
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
# Dependências do Buildroot (manual, seção "System requirements")
apt-get install -y -qq --no-install-recommends \
	build-essential bash bc bzip2 ca-certificates cpio diffutils file \
	findutils git gzip patch perl python3 rsync sed tar unzip wget xz-utils \
	>/dev/null

uid="${HOST_UID:-1000}"
if ! getent passwd builder >/dev/null; then
	# O Buildroot e alguns pacotes (configure) não devem rodar como root
	useradd -m -u "$uid" -o builder
fi
mkdir -p /work
chown builder: /work

exec runuser -u builder -- env WORK=/work BR2_JLEVEL="${BR2_JLEVEL:-}" sh /src/scripts/build.sh
