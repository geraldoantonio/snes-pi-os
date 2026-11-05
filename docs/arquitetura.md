# Arquitetura

## Princípio

Um Linux mínimo que sobe direto no emulador. Nada de login, shell interativo, gerenciador de janelas ou serviços de rede.

```
Boot ROM do Pi → bootloader (start.elf) → kernel → BusyBox init → S99game → launcher → snes9x (PiSNES)
                                                                              ↑__________________|
                                                                  (ao sair do jogo, volta ao menu)
```

## Por que Buildroot

PiSNES é um **programa**, não um sistema operacional. Ele precisa de um Linux por baixo. O Buildroot gera esse Linux sob medida (kernel, BusyBox, bibliotecas e só os pacotes escolhidos), normalmente com dezenas de MB e boot de poucos segundos. Distros como Raspberry Pi OS Lite ou RetroPie carregam muito mais do que o necessário.

## Por que PiSNES

- Feito para o Pi 1; é o SNES9X 1.39 (antigo e leve) com vídeo GLES2/dispmanx.
- Áudio por ALSA; SDL 1.2 usado só para entrada (teclado e joystick).
- Executável isolado: `snes9x <rom>` roda sem o GUI.
- Contrapartida: compatibilidade menor que a de emuladores mais novos (veja [hardware-e-limites.md](hardware-e-limites.md)).

Alternativas consideradas e descartadas para este alvo: RetroArch completo (pesado), snes9x/bsnes atuais (lentos demais em 700 MHz), cores libretro snes9x2002/2005 com frontend próprio (opção viável, deixada como plano B).

## Dependências de runtime do PiSNES

Inferidas da página do projeto e da receita Yocto (a confirmar com o Makefile real):

- `rpi-userland` (EGL, GLES2, dispmanx, bcm_host)
- `sdl` (1.2)
- `alsa-lib`
- `zlib`
- possivelmente `glib` (listado na receita Yocto)

## Armazenamento

| Partição | Sistema de arquivos | Uso | Montagem |
|---|---|---|---|
| 1 | FAT32 | firmware, `config.txt`, `cmdline.txt`, kernel | `/boot` (ro) |
| 2 | ext4 ou squashfs | raiz do sistema | `/` (ro) |
| 3 | FAT32 | ROMs, saves, snapshots | `/roms` (rw) |

A partição de ROMs em FAT32 permite trocar jogos pelo PC sem recompilar nada.

## Saves

O PiSNES guarda SRAM e snapshots em `$HOME/.snes96_snapshots/`. Como a raiz é somente leitura, o `HOME` do processo deve apontar para a partição `/roms` (por exemplo `HOME=/roms`). Isso é definido no `S99game`.

## Desligamento

O aparelho será desligado na tomada. Raiz somente leitura e escrita só em `/roms` (FAT32, com `sync` após salvar) reduzem o risco de corrupção.

## Boot rápido (ideias, a medir)

- `quiet loglevel=0`, sem splash, `boot_delay=0` no `config.txt`
- `gpu_mem` reduzido (a GPU ainda precisa de memória para o dispmanx/GLES; testar valores)
- BusyBox init com um único script de inicialização
- Opcional: usar o launcher como `init` (`init=`) para pular o BusyBox init
