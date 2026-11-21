# AGENTS.md

Orientações para agentes de IA (e pessoas) que trabalham neste repositório. Leia também o [README](README.md) e a pasta [docs/](docs/), começando por [docs/pendencias.md](docs/pendencias.md).

## O projeto em uma frase

Linux mínimo (Buildroot) para o **Raspberry Pi 1 Model B rev 2** que liga direto num launcher próprio em C e roda jogos de SNES no **PiSNES** (SNES9X 1.39, vídeo GLES2/dispmanx). Boot, launcher e jogos já rodam no hardware (2026-11-07); o resto está em `docs/pendencias.md`.

## Retomando o trabalho numa nova sessão

1. Leia este arquivo inteiro, depois `docs/pendencias.md` (decisões e o que falta) e `git log --oneline -20`.
2. Veja o estado do CI e das versões:
   ```sh
   gh run list --limit 10
   gh release list
   ```
3. Confira a seção **Estado atual** abaixo e pergunte ao dono se houve teste no hardware desde então (é a informação que mais muda as prioridades).
4. Antes de mexer em pacotes ou no board, valide a defconfig (ver **Comandos úteis**). Antes de mexer no launcher, rode `make test` e `make shots`.
5. Ao terminar a sessão, **atualize a seção Estado atual** e `docs/pendencias.md`.

## Estado atual

> Atualize esta seção ao fim de cada sessão. Última atualização: **2026-11-21**.

- **Versão publicada:** `v1.0.0` (2026-11-21), a primeira estável (sem `--prerelease`): imagem sem login na serial e sem `hello_pi`, hardware validado no Pi. O firmware do controle arcade tem Release separada, `arcade-v1.0.0` (2026-11-21). A `v0.2.1` (pré-lançamento, 2026-11-20) trouxe o mapeamento de controle USB estilo SNES no jogo (na `v0.2.0`, o Start desses controles gravava o estado sem parar). A `v0.2.0` (2026-11-09) trouxe a ROMS ampliada no primeiro boot, o menu do sistema (desligar, reiniciar, depuração), botão de desligar e LED no GPIO, teclado do jogo remapeado e save gravado no `SIGTERM`. Download também pelo site.
- **Hardware (Pi 1 Model B rev 2, HDMI 1080p, teclado USB, fonte 5 V / 3 A, cartão de 32 GB classe 4):** boot, tela de abertura, launcher, música do menu e **os jogos rodam** (Mortal Kombat II, Aladdin, Donkey Kong Country), com overclock de 900 MHz (`arm_freq=900`, governor `performance`), 37 °C e sem throttling.
- **Problemas resolvidos no caminho** (detalhes em `docs/pendencias.md` e em **Armadilhas**): código 134 (`realpath` com FORTIFY, patch `0005`); tela preta ao abrir jogo (SDL esperando troca de terminal com `tty1` em `KD_GRAPHICS`); lentidão do MK2 a 700 MHz.
- **ROMS ampliada até o fim do cartão no primeiro boot** (`S01roms`): funciona no Pi.
- **Funcionam no Pi:** desligar pelo menu do sistema (Start › Desligar), com `recentes.txt` e favoritos preservados; teclado do jogo remapeado; controle USB estilo SNES (Knup KP-3124) no launcher e no jogo, dois jogadores (`snes9x.cfg` do overlay, 2026-11-20; Bomberman com dois controles sem travar); som no jogo; saves do jogo (gravados ao sair e mantidos depois de desligar); desligar na tomada não corrompeu a ROMS; Aladdin e MK II rodaram sem a lentidão de antes.
- **Botão de desligar no GPIO3:** desliga e religa o Pi (2026-11-20); falta apertar no meio do jogo.
- **Sem teste no hardware:** Start › Reiniciar e LED de status no GPIO17 (esquema em `site/public/img/ligacao-gpio.svg`).
- **Modo rede (2026-11-21, branch `modulo-de-rede`, sem push):** Start › Receber jogos sobe a rede sob demanda (cabo ou adaptador Wi-Fi USB), mostra endereço, PIN e QR code e recebe jogos e capas pela página do `snespi-upload`. Testado só no computador (`make test`, `make run`, script no `ash` do BusyBox, defconfig validada). A imagem compila no Docker local (2026-11-21, `fb87cd1`, 34 MB em `.xz`); falta o build no CI. **Nada disso rodou no Pi ainda.**
- **Controle arcade (Raspberry Pi Pico):** firmware em `firmware/arcade/`, versão 1.0.0 (2026-11-20, Release `arcade-v1.0.0`, separada da imagem), imita o controle USB estilo SNES, dois jogadores num Pico. **Funciona no Pi:** navega no launcher, cada botão dos dois jogadores acende no lugar certo no "Teste - Controles" e Select + Start sai do jogo (testes com fio ao GND). Falta montar com chaves de verdade (ver `docs/controle-arcade.md`; esquema em `site/public/img/ligacao-pico.svg`; seção "Arcade" no site).
- **Em aberto:** atraso intermitente do teclado no Aladdin (uma vez); `recentes.txt` perdido ao desligar na tomada logo depois de abrir um jogo; jogos com chips especiais sem teste; a lentidão crescente de antes não se repetiu, mas a causa não foi achada. Ferramentas de depuração: `diagnostico.txt` e `emulador.log` na partição ROMS; o monitor do emulador vem desligado (Start › Depuração ou o arquivo `.snespi/depurar`).
- **Build local:** Docker com o volume `snespi-work` (ver **Comandos úteis** e a armadilha sobre build incremental); recompila só o launcher em ~1 min.

## Próximos passos (em ordem)

1. **Completar a validação no hardware** (os jogos já rodam). Lista de verificação:
   - o launcher aparece na TV (boot, framebuffer 1280×720, montagem de `/roms`);
   - o controle navega no menu (A do Xbox confirma, B volta, X favorita, Back troca o filtro);
   - **Teste - Controles** acende os botões dos jogadores 1 e 2 no emulador;
   - **Teste - Breakout** tem imagem e som; Back + Start volta ao menu; o save aparece;
   - desligar na tomada e ligar de novo não corrompe a partição ROMS.
   Sem saída na TV, a depuração precisa de adaptador USB-serial (mensagens do kernel em `ttyAMA0`, 115200; não há login).
2. Corrigir o que o teste mostrar. Tudo isso está em `docs/pendencias.md`, na seção "A testar no hardware".
3. **Modo rede:** implementado (ver `docs/launcher.md`). Falta testar no Pi pelo cabo e com um adaptador Wi-Fi USB (lista em `docs/pendencias.md`) e conferir as telas com o canvas.
4. Limpeza antes de uma versão estável: feita (sem login na serial, sem `hello_pi`, `snes9x.cfg` para controles genéricos). O `fsck.fat` no boot ficou de fora: desligar na tomada não corrompeu a ROMS.
5. Releases: o `--prerelease` já saiu do `build.yml`; a próxima tag (`v1.0.0`) sai como versão estável. Depois dela, apontar o botão do site para `releases/latest`.

## Releases

Releases são **públicas** e viram o download da página do projeto. Só crie uma quando o dono pedir.

**Como funciona:** push de uma tag `v*` → o job `image` do `build.yml` compila a imagem e cria a Release com `snes-pi-os-<tag>.img.xz`, `snes-pi-os-<tag>.img.xz.sha256` e as notas de `.github/release-notes.md`. Push na `main` e "Run workflow" manual **não** criam Release (só geram um artefato temporário de 14 dias). O job `image` também roda em push na `main` que mude a imagem (`external/`, `launcher/`, scripts do build): é ele que grava o cache que as tags leem, porque o GitHub só compartilha cache entre refs a partir da `main` (o cache salvo por uma tag só serve para ela; até a `v1.0.0`, toda tag compilava do zero, ~1 h 30). O GitHub apaga caches sem uso há 7 dias: se a `main` ficou mais de uma semana sem build da imagem, rode `gh workflow run build.yml --ref main` e espere terminar antes de criar a tag.

**Firmware do controle arcade:** tem Release própria, separada da imagem, para uma mudança só no firmware não gerar versão nova do sistema (e vice-versa). Push de uma tag `arcade-v<versão>` → o job `firmware` confere se a tag bate com `firmware/arcade/VERSION` e cria a Release "Controle arcade v<versão>" com `snespi-arcade-1p-v<versão>.uf2`, `snespi-arcade-2p-v<versão>.uf2`, o `.sha256` e as notas de `.github/release-notes-arcade.md`. Ela sai com `--latest=false`, para a imagem continuar sendo a versão "mais recente" (o `releases/latest`). Para lançar: suba o `VERSION` e a tabela em `docs/controle-arcade.md`, faça o merge na `main`, crie a tag `arcade-v<versão>` e atualize o link do firmware no site (`site/public/index.html`, seção `#arcade`). Vale a mesma regra: só com o dono pedindo.

**Passo a passo:**

1. Confirme com o dono a versão e se pode publicar.
2. Garanta a `main` verde (`gh run list --branch main --limit 3`) e os docs em dia.
   Se o último build da imagem na `main` tem mais de 7 dias, o cache já expirou: rode `gh workflow run build.yml --ref main` e espere terminar antes da tag.
3. Se precisar, ajuste `.github/release-notes.md` (texto genérico, vale para qualquer versão) e faça commit.
4. Crie e envie a tag anotada:
   ```sh
   git tag -a v0.2.0 -m "snes-pi-os v0.2.0"
   git push origin v0.2.0
   ```
5. Acompanhe o build da tag (`gh run list --limit 3`; ~22 min com o cache da `main`, ~1 h 30 sem cache) e confira a Release:
   ```sh
   gh release view v0.2.0
   ```
6. Atualize **Estado atual** neste arquivo.

**Versionamento:** `0.x.y` enquanto não houver teste completo no hardware. Novo recurso sobe o `x`; só correções sobem o `y`.

**Pré-lançamento:** até a `v0.2.1`, o passo "Criar release" usava `--prerelease`. Com o hardware validado (2026-11-20), a opção saiu do `build.yml`: as próximas tags viram versões normais, o link `releases/latest` passa a funcionar e o botão do site pode apontar para ele.

**Release com problema:** não reaproveite a tag. Corrija e publique a próxima versão. Para apagar uma Release, só com autorização do dono:
```sh
gh release delete v0.2.0 --cleanup-tag
```

## Serviços e infraestrutura

| Serviço | Detalhes |
|---|---|
| GitHub | `geraldoantonio/snes-pi-os`, público. Actions grátis nos runners padrão. Cache de `dl/` e `ccache` (~1,4 GB), gravado pelos builds da `main` e lido pelas tags. |
| Secrets | `CLOUDFLARE_API_TOKEN` (modelo "Edit Cloudflare Workers", conta + zona `geraldojunior.dev`) e `CLOUDFLARE_ACCOUNT_ID`. Quem cria e troca o token é o dono. |
| Página | Worker `snespi-site` (só arquivos estáticos), domínio `snespi.geraldojunior.dev`. Publicada pelo `site.yml` a cada push que mude `site/`. Validar sem publicar: `cd site && npx wrangler@4.158.0 deploy --dry-run`. |
| Design | Canvas "Launcher snes-pi-os" no claude.ai (privado do dono): telas do launcher, modo rede e página de upload. As fontes do canvas ainda usam Pixelify nos títulos; o launcher real usa IBM Plex Mono. |
| Artefatos do CI | `snespi-arcade-<sha>` (firmware do controle arcade, em push ou PR que mude o firmware), `snespi-sdcard-<sha>` (imagem) `qemu-boot-<sha>` (`tela.png` + `serial.log`) e `build-time-<sha>` (`build-time.log` do Buildroot: duração de cada etapa de cada pacote, para ver onde o build gasta tempo), 14 dias. |

## Regras de trabalho

- **Idioma:** docs, comentários de docs, mensagens de commit e textos da interface em **português do Brasil**. Exceção: arquivos `.mk` do Buildroot e patches para projetos de terceiros ficam em inglês e em **ASCII** (o `check-package` recusa UTF-8 em `.mk`).
- **Commits pequenos**, um assunto por commit, mensagem em português.
- **Não faça push nem crie tags sem o dono do repositório pedir.** Tag `v*` publica uma Release pública.
- **Nunca inclua ROMs no repositório** (o `.gitignore` bloqueia `.sfc`, `.smc`, `.fig`, `.swc`, `.zip`). Jogos de teste livres vêm por download no build (pacote `snespi-homebrew`).
- **Marque como "(verificar)"** tudo o que não foi testado, principalmente no hardware. Não afirme que algo funciona no Pi sem teste real.
- **Atualize os docs** quando uma decisão for tomada ou algo mudar, em especial `docs/pendencias.md`.
- **Não invente dependências ou caminhos:** confira no código-fonte (PiSNES, Buildroot 2025.02.19) antes de escrever pacotes.
- Credenciais (tokens do Cloudflare etc.) nunca entram em arquivos, comandos visíveis ou logs; vão como secrets do GitHub, configurados pelo dono.

## Mapa do repositório

| Caminho | O que é |
|---|---|
| `external/` | Árvore BR2_EXTERNAL (`SNESPI`): `configs/snespi_defconfig`, `package/*`, `board/snespi/` |
| `external/package/pisnes/` | PiSNES do fork `RetroPie/pisnes` (`a5b427f`) + patches `0001` a `0007` (cross-compile, correções, logs de diagnóstico, save no `SIGTERM`) |
| `external/package/snes-launcher/` | Compila `launcher/` (site method `local`, sempre com `make -B`) |
| `external/package/snespi-homebrew/` | Baixa exemplos do OpenSNES (MIT) para a partição ROMS |
| `external/board/snespi/` | `config.txt`, `cmdline.txt`, `genimage.cfg.in`, `post-build.sh`, `post-image.sh`, `overlay/` (`fstab`, `S00splash`, `S01roms`, `S99game`, `snespi-frontend`, `snespi-rede` para a rede do modo "Receber jogos", `snes9x.cfg` com o teclado do jogo) |
| `launcher/` | Launcher em C: `src/` (gfx, font, rom, cover, prefs, ui, net, main, plataformas), `upload/` (servidor `snespi-upload` do modo rede e sua página), `fonts/`, `third_party/` (`stb_*`, `qrcodegen`), `tests/` |
| `firmware/arcade/` | Firmware do controle arcade para o Raspberry Pi Pico (pico-sdk 2.3.1 + TinyUSB): `src/pad.c` (filtro e relatório, testado no computador), `src/pins.h` (pinos), `src/usb_descriptors.c` |
| `scripts/` | `build.sh`, `ci-container.sh` (Debian 12), `qemu-smoke.sh` + `qemu_screendump.py`, `build-firmware.sh` + `firmware-container.sh` (controle arcade) |
| `site/` | Página pública estática (`public/`) e `wrangler.jsonc` (Cloudflare Workers, domínio `snespi.geraldojunior.dev`) |
| `.github/workflows/` | `build.yml` (check, firmware do controle arcade e sua Release em tags `arcade-v*`, imagem, Release em tags `v*`, QEMU) e `site.yml` (deploy do site) |
| `.github/release-notes.md` | Texto das Releases (instruções de instalação) |
| `.github/release-notes-arcade.md` | Texto das Releases do firmware do controle arcade (tags `arcade-v*`) |
| `CLAUDE.md` | Só importa este arquivo, para o Claude Code carregá-lo sozinho |

## Comandos úteis

```sh
# Launcher, no computador (macOS/Linux)
cd launcher
make run            # janela SDL2, ROMs de teste, emulador falso (tests/fake-emu.sh)
make test           # cabeçalhos das ROMs, música, servidor de envio (ASan) e modo rede sem tela
make shots          # capturas das telas em build/shots/ (confira o visual por elas)
make clean

# Controle arcade (Pico)
make -C firmware/arcade test   # lógica, no computador
make -C firmware/arcade uf2    # compila no Docker (volume snespi-pico); saída em firmware/arcade/build/
make -C firmware/arcade linux-test   # QEMU no Docker: como o Linux enxerga o controle (uhid)

# Validar a defconfig sem compilar (precisa de uma árvore do Buildroot 2025.02.19)
make -C <buildroot> BR2_EXTERNAL=$PWD/external O=/tmp/out snespi_defconfig

# Imagem completa, local, com Docker
docker run --rm -e HOST_UID=$(id -u) -v "$PWD":/src:ro -v snespi-work:/work \
  debian:12 sh /src/scripts/ci-container.sh

# CI
gh workflow run build.yml --ref main     # build completo + teste no QEMU (grava o cache da main)
```

O job `check` do CI roda o `utils/check-package` do Buildroot em `external/` e valida a defconfig a cada push.

## Armadilhas conhecidas

- **Buildroot fixo em 2025.02.19.** O `rpi-userland` (EGL/GLES/dispmanx da Broadcom, que o PiSNES exige) foi removido na 2026.02. Não atualize a versão sem resolver isso.
- **Placa:** é o Model B (2 USB), não o B+. O firmware procura `bcm2708-rpi-b.dtb`; a defconfig gera os DTBs de B, B rev 1 e B+.
- **Não ative `dtoverlay=vc4-kms-v3d`**: desliga a pilha gráfica legada que o PiSNES usa.
- **PiSNES:**
  - Usa `realpath(argv[0])` e `chdir` para a própria pasta e lê `snes9x.cfg` de lá: execute sempre por **caminho absoluto** (`/usr/share/pisnes/snes9x`).
  - Cai se `HOME` não estiver definido; o frontend usa `HOME=/roms` (saves em `/roms/.snes96_snapshots/`).
  - Super FX, SA-1, C4, SDD-1, SPC7110 e S-RTC não são compilados (`_ZAURUS`). DSP-1 funciona.
  - Sair do jogo: Select + Start no controle 1 (fixo), ou Esc.
  - Link precisa de `-lvchostif` (onde ficam as funções `vc_dispmanx_*`).
- **Raiz somente leitura.** Tudo que precisa persistir vai para `/roms` (FAT32), com escrita atômica (temporário + `fsync` + `rename`), como em `launcher/src/prefs.c`.
- **`genimage.cfg.in`:** os marcadores `#BOOT_FILES#` e `#ROMS_FILES#` são trocados por `sed` no `post-image.sh`. Não os mencione em comentários do arquivo, ou o `sed` troca lá também.
- **Frontend:** roda pelo `inittab` (`tty1::respawn:/usr/bin/snespi-frontend`), não pelo `S99game`, que só carrega módulos. O `post-build.sh` próprio não põe login no `tty1`.
- **Launcher:**
  - RGB565: misture cores canal a canal (`gfx_blend_px`); o truque de máscara empacotada só vale com alfa de 5 bits.
  - Pixelify Sans só no logo: no desenho dela, "C" parece "O" e "B"/"Z" parecem "8". Textos e títulos em IBM Plex Mono.
  - Mapeamento: controles com a convenção do kernel (Xbox etc.) confirmam com `BTN_SOUTH` (A do Xbox), voltam com `BTN_EAST`, favoritam com `BTN_WEST`. Controles genéricos estilo SNES confirmam com o botão da direita.
  - Sem relógio no Pi: "recentes" é por ordem de abertura, nunca por data.
  - Música do menu: o launcher **tem de soltar o ALSA antes de abrir o emulador** (`music_stop()` antes do `fork`), senão o PiSNES encontra o áudio ocupado. A música padrão é CC0 (`launcher/music/`); não troque por faixa sem licença livre.
- **Modo rede:**
  - A rede só existe enquanto a tela "Receber jogos" está aberta. Todo caminho de saída do launcher (B, botão de desligar, `SIGTERM`) tem de passar por `net_stop()`, que para o servidor e chama `snespi-rede down`.
  - O launcher e o `snespi-upload` conversam por linhas com campos separados por TAB na saída padrão (`pronto`, `inicio`, `ok`, `erro`, `bloqueado`), e o `snespi-rede` responde no mesmo formato. Mudou um lado, mude o outro e o `tests/fake-rede.sh`.
  - O `wpa_cli scan_results` escreve bytes fora do ASCII como `\xNN`; o launcher desfaz isso (`decode_ssid`) e o script devolve o nome ao `wpa_supplicant` em hexadecimal. Não monte `ssid="..."` nem `psk="..."` com texto do usuário.
  - A página (`launcher/upload/pagina.html`) vira string em C por `sed`, linha a linha: não use aspas duplas nela (atributos e JavaScript com aspas simples).
  - No Pi a porta é a 80 (o launcher roda como root); no `make run`, 8080.
- **Controle arcade (Pico):** o relatório HID tem de continuar igual ao do controle genérico estilo SNES (coleção *Joystick*, botões X = 0, A = 1, B = 2, Y = 3, L = 4, R = 5, Select = 8, Start = 9, eixos X/Y com centro em 0x7f). Se mudar, o launcher e o `snes9x.cfg` deixam de servir para os dois. O PiSNES só abre os dois primeiros joysticks. A versão do firmware é própria (`firmware/arcade/VERSION`, vai para o `bcdDevice` do USB e para o nome do `.uf2`) e tem tag e Release próprias (`arcade-v*`, ver **Releases**): ao mudar o firmware, suba a versão e a tabela em `docs/controle-arcade.md`.
- **QEMU** (`raspi1ap`) não emula a GPU VideoCore: o PiSNES não abre lá. O teste de boot cobre kernel, init, montagens e o launcher no framebuffer.
  - O QEMU 8.2 também não emula todo o bloco de energia/watchdog (`/soc/watchdog@7e100000`): o kernel cai em `bcm2835_power_probe` ("external abort"). O `qemu-smoke.sh` desliga esse nó numa **cópia** do device tree só para o teste. Falha no QEMU não significa falha no Pi: leia o `serial.log` do artefato `qemu-boot-*` antes de mexer na imagem.
- **`gh secret set` via `!` no Claude Code** não é interativo e grava valor vazio; use `pbpaste | gh secret set NOME` ou a interface do GitHub.
- **`README.md`** às vezes fica com permissão 444 no disco local; o git não guarda isso. Se precisar editar, `chmod u+w` e restaure depois.

- **FORTIFY:** o Buildroot compila com `_FORTIFY_SOURCE`; código antigo (PiSNES) aborta com `buffer overflow detected` em padrões que nunca foram testados assim. Para o PiSNES ele fica desligado no `pisnes.mk`.
- **Build local de outra branch:** o `output` do volume `snespi-work` guarda o estado do último build (kernel, pacotes, `target/`). Para compilar uma branch com defconfig diferente sem misturar os dois, use outra pasta de saída no mesmo volume (`O=/work/output-<nome>`, com `BR2_DL_DIR=/work/dl` e `BR2_CCACHE_DIR=/work/ccache`), como foi feito com `/work/output-rede`.
- **Build local incremental:** mudanças em `config.txt`/`cmdline.txt` só entram com `make rpi-firmware-rebuild all`, e no launcher com `make snes-launcher-rebuild all` (site `local`). O overlay e o `post-image.sh` rodam de novo em todo `make all`.
- **Overclock e governor:** o kernel vem com o governor `powersave` (CPU no mínimo); sem trocar para `performance` (feito no `S99game`), o `arm_freq=900` do `config.txt` não tem efeito.
- **Terminal antes do emulador:** o launcher tem de deixar o `tty1` em `KD_TEXT` antes do `fork` (`video_suspend`). O SDL 1.2 (fbcon) do PiSNES troca de terminal com `VT_ACTIVATE` + `VT_WAITACTIVE`, e o kernel ignora trocas a partir de um terminal em `KD_GRAPHICS`: o emulador trava sem imagem, sem som e sem teclado.
- **Depurar no hardware sem cabo serial:** a partição ROMS recebe `/.snespi/diagnostico.txt` (a cada boot, via `snespi-diagnostico`) e `/.snespi/emulador.log` (acumula os jogos abertos, pelo launcher; passando de 64 KB vira `emulador.log.1`). Por padrão, o `emulador.log` tem só o registro básico (jogo, etapas de partida do PiSNES, erros, código de saída) e o launcher faz um único `fsync` aos 10 s. Com a **depuração ligada** (Start › Depuração, que cria ou apaga o arquivo vazio `/roms/.snespi/depurar`), o monitor (`launcher/src/monitor.c`) anota a cada 5 s a CPU e o `wchan` de cada thread do emulador, carga, temperatura, frequência, memória e mensagens novas do kernel (cerca de 240 KB por hora; para de anotar ao passar de 256 KB numa sessão). Para investigar lentidão ou travamento, peça ao dono para ligar a depuração, reproduzir e trazer esses arquivos antes de supor a causa.
- **Overlays do `config.txt`** (`gpio-shutdown`, `gpio-led`) só funcionam porque a defconfig instala a pasta `overlays/` (`BR2_PACKAGE_RPI_FIRMWARE_INSTALL_DTB_OVERLAYS=y`). Num build local já existente, mudar isso exige `make linux-rebuild rpi-firmware-rebuild all`.
- **Desligar:** quem trata o botão (`KEY_POWER`) é o launcher, não um daemon. Ele precisa do `SIGTERM` com tratamento no PiSNES (patch `0007`) para não perder o save, e sai deixando a tela "Desligando" (`video_detach`) com `SIGTERM` no padrão, para o init matá-lo sem apagar a imagem.
- **Código de saída do emulador no rodapé:** 134 = SIGABRT (assert, geralmente EGL), 139 = SIGSEGV, 127 = não executou (binário ou biblioteca faltando).

## Antes de dar algo por pronto

1. `make test` e `make shots` em `launcher/` passam, e as capturas batem com o design (canvas "Launcher snes-pi-os").
2. A defconfig resolve no kconfig e o `check-package` não reclama.
3. Docs atualizados, com "(verificar)" onde couber.
4. Commit pequeno, em português, e **sem push** até o dono pedir.
