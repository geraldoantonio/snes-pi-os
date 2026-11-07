# Montagem da imagem com Buildroot

> **Nada aqui foi compilado nem testado em hardware ainda.** Itens marcados com **(verificar)** dependem do primeiro build ou do Pi.

## 1. Versão do Buildroot: 2025.02.19 (LTS)

O PiSNES depende do `rpi-userland` (EGL/GLES2/dispmanx/bcm_host da Broadcom). Esse pacote:

- existe no Buildroot até a 2025.11 e na LTS 2025.02.x;
- foi **removido em 30/12/2025** (não compila com CMake ≥ 4; o repositório `raspberrypi/userland` está arquivado e se declara obsoleto);
- não existe na 2026.02 em diante.

Por isso fixamos a **2025.02.19**, a última LTS com o pacote. Ela traz kernel Raspberry Pi 6.6.28 (`bcmrpi_defconfig`, com `BCM2835_VCHIQ=y` e `FB_BCM2708=y`) e toolchain interna do Buildroot.

Alternativas descartadas por enquanto: copiar o `rpi-userland` para `external/` numa versão nova do Buildroot (manter o pacote e corrigir o CMake), ou portar o vídeo do PiSNES para KMS/Mesa `vc4` (patch grande, desempenho desconhecido no Pi 1).

**Risco principal (verificar):** se a pilha GLES legada do firmware ainda funciona no Pi 1 com o kernel 6.6. O primeiro teste é a imagem com `hello_triangle` (seção 6), antes do PiSNES.

## 2. Onde compilar

- **GitHub Actions** (repositório público: runners grátis com 4 vCPU e 16 GB de RAM), dentro de um contêiner Debian 12.
- Localmente, o mesmo contêiner no Docker. O Buildroot exige Linux e um sistema de arquivos que diferencie maiúsculas de minúsculas (num Mac, usar um volume do Docker, não uma pasta compartilhada do macOS) **(verificar)**.

O Debian 12 traz CMake 3.25, o que evita o problema do CMake ≥ 4 com o `rpi-userland`.

### No GitHub Actions

O workflow `.github/workflows/build.yml` tem dois jobs:

- `check` (a cada push na `main` e em PRs, cerca de 1 min): `check-package` nos pacotes e validação da `snespi_defconfig`.
- `image` (só manual, pelo botão "Run workflow", ou em tags `v*`): build completo no contêiner Debian 12, com cache de `dl/` e `ccache`. Publica `sdcard.img.xz` como artefato por 14 dias.

### Localmente (Docker)

```sh
docker run --rm -e HOST_UID=$(id -u) \
  -v "$PWD":/src:ro -v snespi-work:/work \
  debian:12 sh /src/scripts/ci-container.sh
```

O volume `snespi-work` guarda Buildroot, downloads, ccache e `output/`. A imagem fica em `/work/output/images/` dentro do volume.

### À mão (num Linux)

```sh
WORK=/algum/lugar scripts/build.sh
```

Equivale a clonar o Buildroot 2025.02.19, `make BR2_EXTERNAL=.../external snespi_defconfig` e `make`.

Primeira compilação: estimativa de 1 a 3 horas (toolchain + kernel + pacotes) **(verificar)**.

## 3. Configuração (`snespi_defconfig`)

Parte do `raspberrypi_defconfig` da 2025.02.19 e ajusta:

- Pacotes: `rpi-userland`, `rpi-firmware`, `sdl` (1.2, driver fbcon), `alsa-lib`, `libglib2`, `jpeg`, `zlib`, `pisnes`
- Sem DHCP, rede ou SSH; sem ferramentas de desenvolvimento no alvo
- Raiz somente leitura (`BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW` desligado)
- Overlay: `board/snespi/overlay/` (passo 3)

## 4. Pacote `pisnes`

Fork `RetroPie/pisnes`, commit fixo `a5b427f`. Escolhido em vez do `squidrpi/pisnes` porque respeita `$(CFLAGS)` (não fixa flags de CPU) e já tem `-Wno-narrowing`. As diferenças entre os dois forks são pequenas.

O Makefile original precisa de **patch** para cross-compilar:

- `-I/usr/include`, `-I/usr/include/SDL`, `-I/usr/include/glib-2.0`, `-L/usr/lib/arm-linux-gnueabihf` são caminhos do host, recusados pelo wrapper do Buildroot ("unsafe path"); `/opt/vc` não existe no sysroot.
- O patch troca isso por `pkg-config` (glib) e caminhos do sysroot, e adiciona `$(LDFLAGS)` no link.
- O compilador C++ se chama `CCC` (não `CXX`): o `.mk` passa `CC=` e `CCC=`.
- Não há alvo `install`: o `.mk` instala `snes9x`, `snes9x.gui`, `snes9x.cfg` e `skins/` em `/usr/share/pisnes/`.
- Possível erro com GCC recente no C antigo de `unzip/` **(verificar)**.

## 5. Board `external/board/snespi/`

| Arquivo | Função |
|---|---|
| `config.txt` | `gpu_mem=64`, `disable_splash=1`, `boot_delay=0`, `disable_overscan=1`, `dtparam=audio=on` **(verificar se é necessário)**, overclock comentado. **Não** ativar `dtoverlay=vc4-kms-v3d`: ele desliga a pilha legada. |
| `cmdline.txt` | Raiz `/dev/mmcblk0p2` `ro`, consoles `ttyAMA0` e `tty1`, `consoleblank=0`, `vt.global_cursor_default=0`, `loglevel=3`. |
| `genimage.cfg.in` | 3 partições: boot (FAT, 32M), rootfs (ext4, 120M), roms (FAT32, 512M, rótulo `ROMS`). |
| `dir.txt` | Instruções que vão na partição de ROMs. O GUI do PiSNES ignora esse nome. |
| `post-build.sh` | Cria `/roms` e põe o frontend no `inittab`. **Não** chama o `post-build.sh` do raspberrypi, que coloca login no `tty1`. |
| `post-image.sh` | Gera o `genimage.cfg` e a `sdcard.img` (baseado no do raspberrypi). |
| `overlay/etc/fstab` | O do skeleton, com a raiz `ro` em ext4 e `/roms` em vfat (`rw,noatime,flush,utf8`). |
| `overlay/etc/init.d/S99game` | Carrega `snd_bcm2835` e `joydev` (são `=m` no `bcmrpi_defconfig`) e avisa se `/roms` não montou. No `stop`, faz `sync`. |
| `overlay/usr/bin/snespi-frontend` | Define `HOME=/roms` e executa o GUI nativo a partir de `/usr/share/pisnes`. |

Como o frontend sobe: `inittab` → `tty1::respawn:/usr/bin/snespi-frontend`. Pelo init (e não pelo `S99game`), o frontend ganha o `tty1` como terminal e é reiniciado se sair ou cair, sem travar o `rcS`.

Outras escolhas da defconfig:

- `mdev`, para carregar módulos de gamepads (ex.: `xpad`, `hid-sony`) pelo modalias.
- Login (`getty`) só na serial `ttyAMA0` a 115200, para depuração; remover no fim.
- Sem `urandom-scripts` (o `seedrng` grava em `/var/lib`, na raiz somente leitura).

> `gpu_mem=16` é arriscado: o PiSNES usa EGL/GLES2/dispmanx e precisa de memória de GPU. Começar com 64 e reduzir só depois de medir. O `config_default.txt` do Buildroot usa 100.

Pontos a verificar no hardware: se `dtparam=audio=on` é necessário; se o FAT montado com `utf8` mostra nomes com acento; se o cartão "sujo" (desligado na tomada) monta sem `fsck.fat` (ainda não incluído).

## 6. Ordem de validação no hardware

1. Imagem com `BR2_PACKAGE_RPI_USERLAND_HELLO=y`: rodar `hello_triangle`. Se falhar, a pilha GLES legada não funciona e o plano precisa mudar antes de seguir.
2. Imagem com o PiSNES e o GUI nativo (`snes9x.gui`).
3. Imagem com o launcher próprio.

## 7. Gravação no cartão SD

O Buildroot gera `sdcard.img` (via `genimage`) já com a 3ª partição (FAT32, 512M, rótulo `ROMS`). Para mais espaço, aumentar a partição num PC depois de gravar (ex.: GParted), ou mudar o `size` em `genimage.cfg.in`.

## 8. Primeiro boot

1. Gravar a imagem no SD.
2. Copiar ROMs (`.sfc`/`.smc`) para a partição `/roms`.
3. Ligar com um gamepad USB conectado e HDMI ligado.
