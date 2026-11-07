# AGENTS.md

Orientações para agentes de IA (e pessoas) que trabalham neste repositório. Leia também o [README](README.md) e a pasta [docs/](docs/), começando por [docs/pendencias.md](docs/pendencias.md).

## O projeto em uma frase

Linux mínimo (Buildroot) para o **Raspberry Pi 1 Model B rev 2** que liga direto num launcher próprio em C e roda jogos de SNES no **PiSNES** (SNES9X 1.39, vídeo GLES2/dispmanx). Nada foi testado em hardware ainda.

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
| `external/package/pisnes/` | PiSNES do fork `RetroPie/pisnes` (`a5b427f`) + patches `0001` (cross-compile, `-lvchostif`) e `0002` (free fora do array no GUI) |
| `external/package/snes-launcher/` | Compila `launcher/` (site method `local`, sempre com `make -B`) |
| `external/package/snespi-homebrew/` | Baixa exemplos do OpenSNES (MIT) para a partição ROMS |
| `external/board/snespi/` | `config.txt`, `cmdline.txt`, `genimage.cfg.in`, `post-build.sh`, `post-image.sh`, `overlay/` (`fstab`, `S99game`, `snespi-frontend`) |
| `launcher/` | Launcher em C: `src/` (gfx, font, rom, cover, prefs, ui, main, plataformas), `fonts/`, `third_party/stb_*`, `tests/` |
| `scripts/` | `build.sh`, `ci-container.sh` (Debian 12), `qemu-smoke.sh` + `qemu_screendump.py` |
| `site/` | Página pública estática (`public/`) e `wrangler.jsonc` (Cloudflare Workers, domínio `snespi.geraldojunior.dev`) |
| `.github/workflows/` | `build.yml` (check, imagem, Release em tags, QEMU) e `site.yml` (deploy do site) |

## Comandos úteis

```sh
# Launcher, no computador (macOS/Linux)
cd launcher
make run            # janela SDL2, ROMs de teste, emulador falso (tests/fake-emu.sh)
make test           # gera ROMs de teste e confere a leitura dos cabeçalhos
make shots          # capturas das telas em build/shots/ (confira o visual por elas)
make clean

# Validar a defconfig sem compilar (precisa de uma árvore do Buildroot 2025.02.19)
make -C <buildroot> BR2_EXTERNAL=$PWD/external O=/tmp/out snespi_defconfig

# Imagem completa, local, com Docker
docker run --rm -e HOST_UID=$(id -u) -v "$PWD":/src:ro -v snespi-work:/work \
  debian:12 sh /src/scripts/ci-container.sh

# CI
gh workflow run build.yml --ref main     # build completo (~25 min com cache) + teste no QEMU
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
- **QEMU** (`raspi1ap`) não emula a GPU VideoCore: o PiSNES não abre lá. O teste de boot cobre kernel, init, montagens e o launcher no framebuffer.
- **`gh secret set` via `!` no Claude Code** não é interativo e grava valor vazio; use `pbpaste | gh secret set NOME` ou a interface do GitHub.
- **`README.md`** às vezes fica com permissão 444 no disco local; o git não guarda isso. Se precisar editar, `chmod u+w` e restaure depois.

## Antes de dar algo por pronto

1. `make test` e `make shots` em `launcher/` passam, e as capturas batem com o design (canvas "Launcher snes-pi-os").
2. A defconfig resolve no kconfig e o `check-package` não reclama.
3. Docs atualizados, com "(verificar)" onde couber.
4. Commit pequeno, em português, e **sem push** até o dono pedir.
