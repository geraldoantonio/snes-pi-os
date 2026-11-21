# Pendências e decisões em aberto

## Decisões

- [x] **Controle:** USB. Testado com dois controles estilo SNES (Knup KP-3124). Para painel arcade, um Raspberry Pi Pico com firmware próprio (`firmware/arcade/`) que imita esse controle, em vez de ligar os botões no GPIO do Pi (`mk_arcade_joystick_rpi`): não mexe no kernel nem no `snes9x.cfg`, e os controles atuais continuam valendo. Ver [controle-arcade.md](controle-arcade.md).
- [x] **Licença do código original:** MIT (arquivo `LICENSE`). O PiSNES deriva do SNES9X, cuja licença é não comercial; os patches nele seguem a licença do PiSNES, e o launcher continua um programa separado, sem linkar com o emulador.
- [x] **Saída de vídeo principal:** HDMI. O design do launcher é 1280×720 (720p); composto fica sem layout próprio por enquanto.
- [x] **Placa:** Raspberry Pi 1 Model B rev 2 (2 USB, Ethernet), não B+.
- [x] **Desligar com segurança:** menu do sistema no launcher (Start) e, opcionalmente, botão no GPIO3 (pinos 5 e 6) e LED de status no GPIO17 (pinos 11 e 9). Reset fica de fora (furos P6, solda). Ver [hardware-e-limites.md](hardware-e-limites.md).
- [x] **Envio de ROMs:** cartão SD sempre funciona; além disso, um **modo rede** opcional no launcher, com página web de upload pela Ethernet. Desligado por padrão: a rede só sobe enquanto esse modo está aberto. Ver [launcher.md](launcher.md).
- [x] **Launcher:** próprio, em C (`launcher/`); o GUI nativo do PiSNES não é usado.
- [x] **Versão do Buildroot:** 2025.02.19 LTS, a última com `rpi-userland` (removido na 2026.02). Ver [buildroot.md](buildroot.md).
- [x] **Onde compilar:** GitHub Actions (repositório público), em contêiner Debian 12.

## Verificado no código-fonte do PiSNES

- [x] **Fork:** `RetroPie/pisnes`, commit `a5b427f` (respeita `$(CFLAGS)`, usa `libbrcmEGL`/`libbrcmGLESv2`).
- [x] **Bibliotecas:** `bcm_host`, `brcmEGL`, `brcmGLESv2`, `SDL` 1.2, `asound`, `glib-2.0`, `z`, `jpeg`, `rt`, `m`, `pthread`, `stdc++`. Ver [arquitetura.md](arquitetura.md).
- [x] **Cross-compile:** precisa de patch no Makefile (caminhos absolutos do host). A receita Yocto não foi necessária.
- [x] **Sair do jogo:** Select+Start no controle 1, ou Esc. Grava a SRAM ao sair.
- [x] **Config e saves:** `snes9x.cfg` na pasta do binário; saves em `$SNES9X_SNAPSHOT_DIR` ou `$HOME/.snes96_snapshots/`. `HOME` precisa estar definido.

## A verificar no build

- [x] O patch do Makefile basta e o C antigo de `unzip/` compila com o GCC da 2025.02.
- [x] O CMake do contêiner (3.25) compila o `rpi-userland`.
- [x] Espaço em disco e tempo do build no GitHub Actions: cabe depois do passo "Liberar disco"; ~25–30 min com cache.

## Resultado do primeiro teste no hardware (v0.1.0)

- [x] Boot, framebuffer 1280×720, montagem de `/roms`, launcher e teclado USB: funcionam.
- [x] Causa do código 134: não era EGL nem memória da GPU. O `emulador.log` mostrou `*** buffer overflow detected ***`: `realpath()` com buffer de 1000 bytes no `main()` do PiSNES, abortado pelo `_FORTIFY_SOURCE`. Corrigido (patch `0005`, FORTIFY desligado no PiSNES); falta confirmar no Pi.
- [x] GPU funciona: `hello_triangle` e `hello_triangle2` desenham (cubo girando e fractal).
- [x] Áudio: placas HDMI e fone de ouvido detectadas.

Segundo teste (imagem `90b3749`, com o patch `0005`):

- [x] **Música do menu toca** (ALSA funciona no Pi).
- [ ] Ao abrir "Teste - Controles": **tela preta**, LED parado; foi desligado na tomada. O `emulador.log` ficou **vazio** e o `recentes.txt` com zeros: o FAT perdeu o que ainda não tinha ido para o cartão. Não se sabe se travou ou se rodava sem imagem (verificar se Esc volta ao menu e se há som do jogo). Preparado para o próximo teste: patch `0006` (cada etapa da partida do PiSNES no stderr), launcher grava o log no cartão a cada segundo enquanto o emulador roda, `fsync` da pasta depois do `rename` das preferências e `SDL_NOMOUSE=1` para o SDL fbcon não desistir do vídeo (e do teclado) sem mouse.
- [x] Depois: Esc sem efeito e sem som, ou seja, travou antes do loop do jogo. **Causa provável:** o SDL 1.2 (fbcon) do PiSNES troca para um terminal livre (`VT_ACTIVATE` + `VT_WAITACTIVE`), e o kernel ignora trocas a partir de um terminal em `KD_GRAPHICS`, modo em que o launcher deixava o `tty1`. Corrigido: o launcher volta o `tty1` a `KD_TEXT` antes de abrir o emulador.

Terceiro teste (imagem local `d385f29`, 2026-11-07):

- [x] **Os jogos abrem e rodam no PiSNES** pelo launcher. A correção do terminal resolveu a tela preta.
- [x] **Mortal Kombat II roda bem** com a imagem `8f2b625` (900 MHz + governor `performance`). Antes travava; a CPU era o gargalo, não a ROM (que tem a soma de verificação diferente da original).
- [ ] **Aladdin:** roda, música e inimigos seguem, mas às vezes o Aladdin demora a responder ao teclado. Suspeita (verificar): leitura do teclado pelo terminal no SDL 1.2 (fbcon) com várias teclas juntas. Testar com controle USB e, com a depuração ligada (Start › Depuração), ler o monitor no `emulador.log`. Na segunda vez, rodou normal: intermitente.
- [x] **Lentidão crescente até travar, com o som parando** (Mortal Kombat II e Aladdin, já a 900 MHz, 37 °C, sem throttling no boot). Não se repetiu em 2026-11-20 com a `v0.2.x` e controles USB: Bomberman mais de 15 min com dois jogadores, Aladdin e Mortal Kombat II rodaram normal. Causa não identificada; se voltar, ligar a depuração. O log vai até `entering main loop`, sem erro. O Aladdin é leve, o que aponta para o sistema, não para a ROM. Com a depuração ligada (Start › Depuração), o launcher grava no log, a cada 5 s, a CPU e o estado de cada thread do emulador e as mensagens do kernel.
- [x] Som no jogo, controles dos jogadores 1 e 2, Select + Start voltando ao menu e saves funcionam (2026-11-20).

## A testar no hardware

- [x] `hello_triangle` (pilha GLES legada com kernel 6.6 no Pi 1): funciona (ver acima).
- [x] Estabilidade e temperatura com overclock (37 °C, sem throttling; jogos rodando 15 min ou mais sem travar, 2026-11-20) (`arm_freq=900`, `sdram_freq=500`, sugeridos pelo readme do PiSNES). **Ativado por padrão** depois de Mortal Kombat II ficar lento a 700 MHz, com o governor `performance` no `S99game`. O `diagnostico.txt` mostra a frequência, a temperatura e o `get_throttled`.
- [ ] Menor `gpu_mem` que ainda funciona com dispmanx/GLES2.
- [ ] Se `dtparam=audio=on` é necessário para o áudio.
- [ ] Tempo de boot até o menu.
- [ ] Tela de abertura (`S00splash` → `S99game` → launcher): se aparece logo no começo do boot, se a barra avança sem texto do console por cima e quanto tempo cada `--splash` acrescenta ao boot.
- [ ] Jogos com chips especiais: pelo Makefile, Super FX, SA-1, C4, SDD-1, SPC7110 e S-RTC não são compilados. Ver [hardware-e-limites.md](hardware-e-limites.md).
- [x] Raiz somente leitura + `HOME=/roms`: o save do jogo fica gravado e volta depois de desligar (2026-11-20).
- [ ] Se um link simbólico `snes9x.cfg` → `/roms/snes9x.cfg` funciona.
- [x] O FAT de `/roms` monta depois de desligar na tomada, sem corromper (2026-11-20, sem `fsck.fat`). Falta ver se `utf8` mostra acentos nos nomes.
- [x] Gamepad USB genérico: carrega pelo `mdev`; o `snes9x.cfg` do overlay é para ele.
- [x] Removidos da defconfig: login na serial `ttyAMA0` e `hello_pi` (o kernel ainda escreve as mensagens de boot na serial). Verificar na próxima imagem que o boot segue igual.
- [ ] Se o PiSNES abre ROMs `.zip` (o Makefile compila com `UNZIP=1`).
- [ ] Modo rede: tempo do DHCP, velocidade de upload pela Ethernet 10/100 e gravação no FAT.
- [x] **Desligar pelo menu do sistema** (Start › Desligar): funciona no Pi (2026-11-09).
- [ ] Start › Reiniciar no Pi.
- [x] **Botão de desligar** (GPIO3, pinos 5 e 6, botão de gabinete de PC): desliga e religa o Pi parado (2026-11-20), então o `gpio_keys` carrega sozinho. Falta conferir apertando no meio do jogo: o save gravado (patch `0007`, a linha "quit signal, saving SRAM" no `emulador.log`).
- [x] **Teclado no jogo remapeado** (`snes9x.cfg` do overlay): A = C, B = X, X = D, Y = S, L = A, R = Z, Start = Enter, Select = Espaço; Esc ainda sai. Funciona no Pi (2026-11-09).
- [ ] **LED de status** (GPIO17, pinos 11 e 9, ainda não montado): pisca no boot, aceso no menu, pisca ao desligar e apaga no fim.
- [x] **Ampliação da ROMS no primeiro boot** (`S01roms`): funciona no Pi (2026-11-09, cartão de 32 GB): 512 MB → 30.284 MB, arquivos devolvidos, e o diagnóstico (que roda depois) já estava pronto com 22 s de boot, então a formatação é rápida. Depois disso, os dois jogos de teste abriram e saíram com código 0, e `recentes.txt`/`favoritos.txt` ficaram gravados.
- [ ] **Controles USB estilo SNES** (2 × Knup KP-3124, 2026-11-20): navegam no launcher. No Mortal Kombat II, o Start ficava "salvando" e o jogo não começava: o `snes9x.cfg` tinha o mapeamento de Xbox 360, com `QSAVE=9` (o Start destes controles). Trocado pelo mapeamento estilo SNES, com quick save/load só por Select + R / Select + L. No "Teste - Controles" (imagem local `fef9dc5-joy`), o controle funcionou (2026-11-20); no Mortal Kombat II também (Start entra no jogo). Os dois controles funcionam (jogadores 1 e 2): Bomberman com dois jogadores por mais de 15 min, sem travar nem ficar lento (2026-11-20). No MK II, o R não faz nada no jogo, mas acende no "Teste - Controles": é a configuração de botões do próprio jogo.
- [ ] **Controle arcade com Pico** (`firmware/arcade/`, 2026-11-20): compila, passa nos testes da lógica e foi gravado num Pico; no macOS, os 24 pinos dão a entrada certa no jogador certo (teste com fio ao GND). Falta montar com chaves de verdade e testar no Pi. Lista em [controle-arcade.md](controle-arcade.md).
- [x] Depois de desligar pelo menu, `recentes.txt`, `favoritos.txt` e a partição ROMS ficaram íntegros (2026-11-09). Desligar na tomada ainda pode perder o `recentes.txt`.

## Plano B

Se a compatibilidade do PiSNES (SNES9X 1.39) for insuficiente, ou se a pilha GLES legada não funcionar: frontend libretro próprio, carregando `snes9x2002` ou `snes9x2005`, com vídeo por KMS/DRM (ou dispmanx, se disponível) e áudio ALSA. Mais trabalho, mais flexível.

## Próximos passos de implementação

1. [x] Criar `external/` (BR2_EXTERNAL) com `external.desc`, `external.mk` e `Config.in`.
2. [x] Escrever `package/pisnes` com o patch do Makefile.
3. [x] Criar `snespi_defconfig`.
4. [x] Workflow do GitHub Actions.
5. [x] Primeiro build no CI e correções.
6. [x] Overlay (`S99game`, `config.txt`, `cmdline.txt`, `fstab`) e layout do `genimage` com a 3ª partição.
7. [x] Imagem de teste com `hello_triangle`.
8. [x] ~~Primeira imagem: PiSNES com o GUI nativo~~: pulado, o boot foi validado direto com o launcher.
9. [x] Launcher em C e pacote `snes-launcher`.
10. [x] Testar o launcher no hardware: framebuffer em 1280×720, controles USB, volta do emulador. Usar os jogos de teste da partição ROMS (Controles e Breakout, do OpenSNES).
11. [ ] Modo rede: `snespi-upload`, QR code e tela no launcher (como item do menu do sistema, aberto pelo Start).
12. [ ] (Ideia, adiada) Buscar capas pela internet no launcher, junto com o modo rede: índice do `libretro-thumbnails` gerado no build, rede e hora (`udhcpc`, `ntpd -q`) sob demanda, HTTPS com libcurl e certificados, gravação em `/roms/capas`. Um script para PC também ficou para depois.
