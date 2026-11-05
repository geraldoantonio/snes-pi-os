# Montagem da imagem com Buildroot

> **Planejado, ainda não testado.** Os comandos abaixo são o roteiro previsto; nada aqui foi compilado. Itens marcados com **(verificar)** dependem de conferir o código-fonte do PiSNES ou a versão do Buildroot.

## 1. Preparação

```sh
git clone https://git.buildroot.net/buildroot
cd buildroot
git checkout <tag estável mais recente>   # (verificar) escolher uma release LTS
```

Compilar em um PC Linux (x86_64). Contar com algumas dezenas de minutos até algumas horas na primeira vez.

## 2. Configuração base

```sh
make BR2_EXTERNAL=/caminho/para/snes-pi-os/external snespi_defconfig
make menuconfig
```

A `snespi_defconfig` (a criar) parte do `raspberrypi_defconfig` do Buildroot e ajusta:

- Toolchain compatível com ARMv6 hard-float
- Init: BusyBox
- Pacotes: `rpi-userland`, `rpi-firmware`, `sdl` (1.2), `alsa-lib`, `zlib`, `pisnes`, `snes-launcher`
- Sistema de arquivos raiz somente leitura (`BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW` desligado)
- Overlay: `board/snespi/overlay/`
- Sem rede, sem SSH, sem ferramentas de desenvolvimento no alvo

## 3. Pacote `pisnes` (BR2_EXTERNAL)

`external/package/pisnes/pisnes.mk` deve:

1. Baixar o código de `RetroPie/pisnes` (fixar um commit **(verificar)**).
2. Compilar com o compilador cruzado do Buildroot, apontando para as bibliotecas do `rpi-userland` **(verificar o Makefile do projeto; pode exigir patch para cross-compile, como a receita Yocto)**.
3. Instalar o binário `snes9x` em `/usr/bin`.

Dependências a declarar: `rpi-userland`, `sdl`, `alsa-lib`, `zlib`, possivelmente `libglib2`.

## 4. Arquivos de inicialização (overlay)

- `S99game`: script do BusyBox init que define `HOME=/roms` e inicia o launcher.
- `config.txt`: `gpu_mem`, `disable_splash=1`, `boot_delay=0`, overclock opcional.
- `cmdline.txt`: `quiet loglevel=0 fsck.mode=skip ro` e a raiz correta.
- `fstab`: `/roms` em FAT32 (rw, `sync` ou `flush`), raiz em `ro`.

Exemplo inicial de `config.txt` (valores a testar):

```
gpu_mem=64
disable_splash=1
boot_delay=0
force_turbo=0
# arm_freq=800    # overclock: testar estabilidade e temperatura
```

> `gpu_mem=16`, sugerido numa conversa anterior, é arriscado aqui: o PiSNES usa EGL/GLES2/dispmanx e precisa de memória de GPU. Começar com 64 e reduzir só depois de medir.

## 5. Gravação no cartão SD

O Buildroot gera `sdcard.img` (via `genimage`). A partição `/roms` (FAT32) precisa ser adicionada ao layout do `genimage` **(verificar)**, ou criada manualmente após gravar a imagem.

## 6. Primeiro boot

1. Gravar a imagem no SD.
2. Copiar ROMs (`.sfc`/`.smc`) para a partição `/roms`.
3. Ligar com um gamepad USB conectado e HDMI ligado.
