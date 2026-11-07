# Pendências e decisões em aberto

## Decisões

- [ ] **Controle:** USB (assumido) ou GPIO (`mk_arcade_joystick_rpi`)?
- [x] **Licença do código original:** MIT (arquivo `LICENSE`). O PiSNES deriva do SNES9X, cuja licença é não comercial; os patches nele seguem a licença do PiSNES, e o launcher continua um programa separado, sem linkar com o emulador.
- [ ] **Saída de vídeo principal:** HDMI ou composto?
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

## A testar no hardware

- [ ] **Primeiro de tudo:** `hello_triangle` (pilha GLES legada com kernel 6.6 no Pi 1).
- [ ] Estabilidade e temperatura com overclock (`arm_freq=900`, `sdram_freq=500`, sugeridos pelo readme do PiSNES).
- [ ] Menor `gpu_mem` que ainda funciona com dispmanx/GLES2.
- [ ] Se `dtparam=audio=on` é necessário para o áudio.
- [ ] Tempo de boot até o menu.
- [ ] Jogos com chips especiais: pelo Makefile, Super FX, SA-1, C4, SDD-1, SPC7110 e S-RTC não são compilados. Ver [hardware-e-limites.md](hardware-e-limites.md).
- [ ] Efeito de raiz somente leitura + `HOME=/roms` nos saves.
- [ ] Se um link simbólico `snes9x.cfg` → `/roms/snes9x.cfg` funciona.

## Plano B

Se a compatibilidade do PiSNES (SNES9X 1.39) for insuficiente, ou se a pilha GLES legada não funcionar: frontend libretro próprio, carregando `snes9x2002` ou `snes9x2005`, com vídeo por KMS/DRM (ou dispmanx, se disponível) e áudio ALSA. Mais trabalho, mais flexível.

## Próximos passos de implementação

1. [x] Criar `external/` (BR2_EXTERNAL) com `external.desc`, `external.mk` e `Config.in`.
2. [x] Escrever `package/pisnes` com o patch do Makefile (patch testado só com `--dry-run`).
3. [x] Criar `snespi_defconfig` (validada pelo kconfig; não compilada).
4. [x] Workflow do GitHub Actions (ainda não executado).
5. [ ] Primeiro build no CI e correções.
6. [ ] Overlay (`S99game`, `config.txt`, `cmdline.txt`, `fstab`) e layout do `genimage` com a 3ª partição.
7. [ ] Imagem de teste com `hello_triangle`.
8. [ ] Primeira imagem: PiSNES com o GUI nativo, para validar o boot.
9. [ ] Escrever o launcher mínimo e o pacote `snes-launcher`.
