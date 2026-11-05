# snes-pi-os

Sistema Linux mínimo e dedicado para rodar jogos de Super Nintendo (SNES) no **Raspberry Pi 1 Model B+** (BCM2835, ARM1176 @ 700 MHz, 512 MB de RAM).

Não é uma distro genérica: o objetivo é ligar o aparelho, cair direto num menu de ROMs e jogar, com o menor boot e o menor uso de recursos possível.

> Status: **planejamento / documentação**. Ainda não há código nem imagem compilada. Veja [docs/pendencias.md](docs/pendencias.md).

## Visão geral

| Camada | Escolha |
|---|---|
| Hardware alvo | Raspberry Pi 1 B+ (ARMv6, hard-float) |
| Sistema base | Linux montado com [Buildroot](https://buildroot.org/) (BusyBox init, sem systemd) |
| Emulador | [PiSNES](https://github.com/RetroPie/pisnes) (baseado no SNES9X 1.39) |
| Frontend | Launcher mínimo próprio (lista ROMs, executa o emulador, volta ao menu) |
| Vídeo | GLES2/dispmanx (via `rpi-userland`), escalonamento pela GPU |
| Áudio | ALSA (HDMI ou jack 3,5 mm) |
| Controles | Gamepad USB (evdev/SDL 1.2); GPIO opcional via `mk_arcade_joystick_rpi` |
| ROMs e saves | Segunda partição FAT32 do cartão SD, montada em `/roms` |
| Raiz do sistema | Somente leitura (seguro contra desligar na tomada) |

## Documentação

- [docs/arquitetura.md](docs/arquitetura.md): como as peças se encaixam e por que cada escolha foi feita
- [docs/hardware-e-limites.md](docs/hardware-e-limites.md): limites do Pi 1 e compatibilidade de jogos
- [docs/buildroot.md](docs/buildroot.md): como montar a imagem (passo a passo planejado)
- [docs/launcher.md](docs/launcher.md): especificação do frontend mínimo
- [docs/pendencias.md](docs/pendencias.md): decisões em aberto e itens a verificar

## Estrutura planejada do repositório

```
snes-pi-os/
├── README.md
├── docs/
├── external/                  # árvore BR2_EXTERNAL do Buildroot (a criar)
│   ├── package/pisnes/
│   ├── package/snes-launcher/
│   ├── configs/snespi_defconfig
│   └── board/snespi/overlay/  # S99game, config.txt, cmdline.txt, fstab
└── launcher/                  # código C do launcher (a criar)
```

## Jogos e ROMs

Este projeto **não inclui nem distribui ROMs**. Use apenas jogos que você tem o direito de usar.

## Licenças

O PiSNES é GPLv2 (derivado do SNES9X, que tem licença própria não comercial). A licença do código original deste repositório ainda precisa ser escolhida; veja [docs/pendencias.md](docs/pendencias.md).
