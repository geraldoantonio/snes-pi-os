# Pendências e decisões em aberto

## Decisões

- [ ] **Controle:** USB (assumido) ou GPIO (`mk_arcade_joystick_rpi`)?
- [x] **Licença do código original:** MIT (arquivo `LICENSE`). O PiSNES deriva do SNES9X, cuja licença é não comercial; os patches nele seguem a licença do PiSNES, e o launcher continua um programa separado, sem linkar com o emulador.
- [ ] **Saída de vídeo principal:** HDMI ou composto?
- [ ] **Launcher:** começar com o GUI nativo do PiSNES e trocar depois, ou já escrever o launcher próprio?

## A verificar no código-fonte do PiSNES

Não foi possível ler o Makefile do repositório durante o planejamento. Antes de escrever `pisnes.mk`:

- [ ] Qual fork usar: `RetroPie/pisnes` ou `squidrpi/pisnes` (a página do SourceForge diz que o código foi para o segundo; o RetroPie mantém um fork).
- [ ] Lista exata de bibliotecas linkadas (SDL 1.2, EGL, GLESv2, bcm_host, asound, glib?).
- [ ] Se precisa de patch para cross-compilar (a receita Yocto do projeto aplica `cross_compile.patch`).
- [ ] Teclas/botões padrão e como sair do jogo.
- [ ] Onde ele procura configuração e saves (`$HOME/.snes96_snapshots/` segundo o readme).

## A testar no hardware

- [ ] Estabilidade e temperatura com overclock (`arm_freq`).
- [ ] Menor `gpu_mem` que ainda funciona com dispmanx/GLES2.
- [ ] Tempo de boot até o menu.
- [ ] Jogos com chips especiais (Super FX, SA-1) e os que a página do projeto cita como incompatíveis.
- [ ] Efeito de raiz somente leitura + `HOME=/roms` nos saves.

## Plano B

Se a compatibilidade do PiSNES (SNES9X 1.39) for insuficiente: frontend libretro próprio, carregando `snes9x2002` ou `snes9x2005`, com vídeo por dispmanx e áudio ALSA. Mais trabalho, mais flexível.

## Próximos passos de implementação

1. Criar `external/` (BR2_EXTERNAL) com `external.desc`, `external.mk` e `Config.in`.
2. Escrever `package/pisnes` depois de conferir o Makefile.
3. Criar `snespi_defconfig` e o overlay (`S99game`, `config.txt`, `cmdline.txt`, `fstab`).
4. Primeira imagem: PiSNES com o GUI nativo, para validar o boot.
5. Escrever o launcher mínimo.
