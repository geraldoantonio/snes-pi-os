# Arquitetura

## Princípio

Um Linux mínimo que sobe direto no emulador. Nada de login, shell interativo, gerenciador de janelas ou serviços de rede.

```
Boot ROM do Pi → bootloader (start.elf) → kernel → BusyBox init → rcS (S00splash, S01roms … S99game) 
                                                                → inittab tty1::respawn → frontend → snes9x (PiSNES)
                                                                                              ↑__________________|
                                                                              (ao sair do jogo, volta ao menu)
```

O `S00splash` desenha a tela de abertura (logo e carregamento) pelo próprio launcher (`--splash`). O `S01roms` amplia a partição ROMS até o fim do cartão no primeiro boot (ver [buildroot.md](buildroot.md)). O `S99game` só prepara o hardware (módulos de áudio e joystick) e avança a barra. O frontend é iniciado pelo init com `respawn` no `tty1`: ganha o terminal da tela e volta sozinho se cair. Na primeira versão o frontend é o GUI nativo do PiSNES (`snespi-frontend` → `snes9x.gui`); depois, o launcher próprio.

## Por que Buildroot

PiSNES é um **programa**, não um sistema operacional. Ele precisa de um Linux por baixo. O Buildroot gera esse Linux sob medida (kernel, BusyBox, bibliotecas e só os pacotes escolhidos), normalmente com dezenas de MB e boot de poucos segundos. Distros como Raspberry Pi OS Lite ou RetroPie carregam muito mais do que o necessário.

## Por que PiSNES

- Feito para o Pi 1; é o SNES9X 1.39 (antigo e leve) com vídeo GLES2/dispmanx.
- Áudio por ALSA; SDL 1.2 usado só para entrada (teclado e joystick).
- Executável isolado: `snes9x <rom>` roda sem o GUI.
- Contrapartida: compatibilidade menor que a de emuladores mais novos (veja [hardware-e-limites.md](hardware-e-limites.md)).

Alternativas consideradas e descartadas para este alvo: RetroArch completo (pesado), snes9x/bsnes atuais (lentos demais em 700 MHz), cores libretro snes9x2002/2005 com frontend próprio (opção viável, deixada como plano B).

## Dependências do PiSNES

Confirmadas no Makefile e no código do fork `RetroPie/pisnes` (commit `a5b427f`, 2017):

| Biblioteca | Pacote Buildroot | Uso |
|---|---|---|
| `bcm_host`, `brcmEGL`, `brcmGLESv2` | `rpi-userland` | vídeo (dispmanx + GLES2) |
| `SDL` 1.2 | `sdl` (com driver fbcon) | só entrada; chama `SDL_SetVideoMode` |
| `asound` | `alsa-lib` | áudio, dispositivo ALSA `default` |
| `glib-2.0` | `libglib2` | leitura do `snes9x.cfg` (`g_key_file`) |
| `z` | `zlib` | ROMs compactadas |
| `jpeg` | `jpeg` | só o GUI (box art), mas linkado nos dois binários |
| `rt`, `m`, `pthread`, `stdc++` | toolchain | |

O `rpi-userland` foi **removido do Buildroot em 2026.02** (não compila com CMake ≥ 4, e o projeto upstream foi arquivado como obsoleto). Por isso a base é a **2025.02.19**, a última LTS que ainda o contém. Ver [buildroot.md](buildroot.md).

## Comportamento do `snes9x` que afeta o sistema

- Faz `realpath(argv[0])` e `chdir` para a pasta do binário. O launcher deve executá-lo por **caminho absoluto**, nunca pelo `PATH`.
- Lê `snes9x.cfg` da pasta do binário. Sem o arquivo, usa os padrões (mapeamento de controle de Xbox 360).
- ROMs passadas por caminho absoluto são carregadas diretamente.
- Sai com **Select+Start** no controle 1 (fixo no código) ou Esc no teclado, gravando a SRAM antes de sair. Também grava a SRAM automaticamente (`AutoSaveDelay=30`).

## Armazenamento

| Partição | Sistema de arquivos | Uso | Montagem |
|---|---|---|---|
| 1 | FAT32 | firmware, `config.txt`, `cmdline.txt`, kernel | `/boot` (ro) |
| 2 | ext4 ou squashfs | raiz do sistema | `/` (ro) |
| 3 | FAT32 | ROMs, saves, snapshots | `/roms` (rw) |

A partição de ROMs em FAT32 permite trocar jogos pelo PC sem recompilar nada.

## Saves

O PiSNES guarda SRAM (`.srm`) e snapshots em `$SNES9X_SNAPSHOT_DIR` ou, se não definido, em `$HOME/.snes96_snapshots/` (que ele cria). Como a raiz é somente leitura, o `S99game` define `HOME=/roms`.

`HOME` é **obrigatório**: sem ele e sem `SNES9X_SNAPSHOT_DIR`, o código faz `strcpy` de um ponteiro nulo e o emulador cai.

O `snes9x.cfg` fica na raiz (somente leitura) ao lado do binário. Para poder editá-lo pelo PC, ele pode virar um link simbólico para `/roms/snes9x.cfg` **(verificar)**.

## Desligamento

O aparelho pode ser desligado na tomada. Raiz somente leitura e escrita só em `/roms` (FAT32, com `sync` após salvar) reduzem o risco de corrupção.

O jeito seguro é pelo launcher: **Start › Desligar**, ou o botão de desligar no GPIO3 (opcional, ver [hardware-e-limites.md](hardware-e-limites.md)). O launcher fecha o emulador com `SIGTERM` (o PiSNES grava a SRAM, patch `0007`) e chama o `poweroff` do BusyBox: o init roda o `rcK` (`S99game stop` faz `sync`), desmonta tudo (`umount -a -r`) e só depois mata os processos. O LED de status (GPIO17) apaga no fim.

## Boot rápido (ideias, a medir)

- `quiet loglevel=0`, sem splash, `boot_delay=0` no `config.txt`
- `gpu_mem` reduzido (a GPU ainda precisa de memória para o dispmanx/GLES; testar valores)
- BusyBox init com um único script de inicialização
- Opcional: usar o launcher como `init` (`init=`) para pular o BusyBox init
