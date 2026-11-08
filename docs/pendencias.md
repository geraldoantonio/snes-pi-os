# Pendências e decisões em aberto

## Decisões

- [ ] **Controle:** USB (assumido) ou GPIO (`mk_arcade_joystick_rpi`)?
- [x] **Licença do código original:** MIT (arquivo `LICENSE`). O PiSNES deriva do SNES9X, cuja licença é não comercial; os patches nele seguem a licença do PiSNES, e o launcher continua um programa separado, sem linkar com o emulador.
- [x] **Saída de vídeo principal:** HDMI. O design do launcher é 1280×720 (720p); composto fica sem layout próprio por enquanto.
- [x] **Placa:** Raspberry Pi 1 Model B rev 2 (2 USB, Ethernet), não B+.
- [x] **Envio de ROMs:** cartão SD sempre funciona; além disso, um **modo rede** opcional no launcher, com página web de upload pela Ethernet. Desligado por padrão: a rede só sobe enquanto esse modo está aberto. Ver [launcher.md](launcher.md).
- [ ] **Launcher:** começar com o GUI nativo do PiSNES e trocar depois, ou já escrever o launcher próprio?
- [x] **Versão do Buildroot:** 2025.02.19 LTS, a última com `rpi-userland` (removido na 2026.02). Ver [buildroot.md](buildroot.md).
- [x] **Onde compilar:** GitHub Actions (repositório público), em contêiner Debian 12.

## Verificado no código-fonte do PiSNES

- [x] **Fork:** `RetroPie/pisnes`, commit `a5b427f` (respeita `$(CFLAGS)`, usa `libbrcmEGL`/`libbrcmGLESv2`).
- [x] **Bibliotecas:** `bcm_host`, `brcmEGL`, `brcmGLESv2`, `SDL` 1.2, `asound`, `glib-2.0`, `z`, `jpeg`, `rt`, `m`, `pthread`, `stdc++`. Ver [arquitetura.md](arquitetura.md).
- [x] **Cross-compile:** precisa de patch no Makefile (caminhos absolutos do host). A receita Yocto não foi necessária.
- [x] **Sair do jogo:** Select+Start no controle 1, ou Esc. Grava a SRAM ao sair.
- [x] **Config e saves:** `snes9x.cfg` na pasta do binário; saves em `$SNES9X_SNAPSHOT_DIR` ou `$HOME/.snes96_snapshots/`. `HOME` precisa estar definido.

## A verificar no build

- [ ] Se o patch do Makefile basta e se o C antigo de `unzip/` compila com o GCC da 2025.02.
- [ ] Se o CMake do contêiner (3.25) compila o `rpi-userland` sem problemas.
- [ ] Espaço em disco e tempo do build no GitHub Actions.

## Resultado do primeiro teste no hardware (v0.1.0)

- [x] Boot, framebuffer 1280×720, montagem de `/roms`, launcher e teclado USB: funcionam.
- [x] Causa do código 134: não era EGL nem memória da GPU. O `emulador.log` mostrou `*** buffer overflow detected ***`: `realpath()` com buffer de 1000 bytes no `main()` do PiSNES, abortado pelo `_FORTIFY_SOURCE`. Corrigido (patch `0005`, FORTIFY desligado no PiSNES); falta confirmar no Pi.
- [x] GPU funciona: `hello_triangle` e `hello_triangle2` desenham (cubo girando e fractal).
- [x] Áudio: placas HDMI e fone de ouvido detectadas.

Segundo teste (imagem `90b3749`, com o patch `0005`):

- [x] **Música do menu toca** (ALSA funciona no Pi).
- [ ] Ao abrir "Teste - Controles": **tela preta**, LED parado; foi desligado na tomada. O `emulador.log` ficou **vazio** e o `recentes.txt` com zeros: o FAT perdeu o que ainda não tinha ido para o cartão. Não se sabe se travou ou se rodava sem imagem (verificar se Esc volta ao menu e se há som do jogo). Preparado para o próximo teste: patch `0006` (cada etapa da partida do PiSNES no stderr), launcher grava o log no cartão a cada segundo enquanto o emulador roda, `fsync` da pasta depois do `rename` das preferências e `SDL_NOMOUSE=1` para o SDL fbcon não desistir do vídeo (e do teclado) sem mouse.

## A testar no hardware

- [ ] **Primeiro de tudo:** `hello_triangle` (pilha GLES legada com kernel 6.6 no Pi 1).
- [ ] Estabilidade e temperatura com overclock (`arm_freq=900`, `sdram_freq=500`, sugeridos pelo readme do PiSNES).
- [ ] Menor `gpu_mem` que ainda funciona com dispmanx/GLES2.
- [ ] Se `dtparam=audio=on` é necessário para o áudio.
- [ ] Tempo de boot até o menu.
- [ ] Tela de abertura (`S00splash` → `S99game` → launcher): se aparece logo no começo do boot, se a barra avança sem texto do console por cima e quanto tempo cada `--splash` acrescenta ao boot.
- [ ] Jogos com chips especiais: pelo Makefile, Super FX, SA-1, C4, SDD-1, SPC7110 e S-RTC não são compilados. Ver [hardware-e-limites.md](hardware-e-limites.md).
- [ ] Efeito de raiz somente leitura + `HOME=/roms` nos saves.
- [ ] Se um link simbólico `snes9x.cfg` → `/roms/snes9x.cfg` funciona.
- [ ] Se o FAT de `/roms` monta depois de desligar na tomada (sem `fsck.fat` por enquanto) e se `utf8` mostra acentos.
- [ ] Gamepad USB genérico: se carrega pelo `mdev` e qual `snes9x.cfg` ele precisa.
- [ ] Remover depois: login na serial `ttyAMA0` e `hello_pi`.
- [ ] Se o PiSNES abre ROMs `.zip` (o Makefile compila com `UNZIP=1`).
- [ ] Modo rede: tempo do DHCP, velocidade de upload pela Ethernet 10/100 e gravação no FAT.

## Plano B

Se a compatibilidade do PiSNES (SNES9X 1.39) for insuficiente, ou se a pilha GLES legada não funcionar: frontend libretro próprio, carregando `snes9x2002` ou `snes9x2005`, com vídeo por KMS/DRM (ou dispmanx, se disponível) e áudio ALSA. Mais trabalho, mais flexível.

## Próximos passos de implementação

1. [x] Criar `external/` (BR2_EXTERNAL) com `external.desc`, `external.mk` e `Config.in`.
2. [x] Escrever `package/pisnes` com o patch do Makefile (patch testado só com `--dry-run`).
3. [x] Criar `snespi_defconfig` (validada pelo kconfig; não compilada).
4. [x] Workflow do GitHub Actions (ainda não executado).
5. [ ] Primeiro build no CI e correções.
6. [x] Overlay (`S99game`, `config.txt`, `cmdline.txt`, `fstab`) e layout do `genimage` com a 3ª partição (não compilados).
7. [ ] Imagem de teste com `hello_triangle`.
8. [ ] Primeira imagem: PiSNES com o GUI nativo, para validar o boot.
9. [x] Launcher em C e pacote `snes-launcher` (primeira versão; testado só com capturas fora do Pi).
10. [ ] Testar o launcher no hardware: framebuffer em 1280×720, controles USB, volta do emulador. Usar os jogos de teste da partição ROMS (Controles e Breakout, do OpenSNES).
11. [ ] Modo rede: `snespi-upload`, QR code e tela no launcher.
