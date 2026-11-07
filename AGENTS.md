# AGENTS.md

Orientações para agentes de IA (e pessoas) que trabalham neste repositório. Leia também o [README](README.md) e a pasta [docs/](docs/), começando por [docs/pendencias.md](docs/pendencias.md).

## O projeto em uma frase

Linux mínimo (Buildroot) para o **Raspberry Pi 1 Model B rev 2** que liga direto num launcher próprio em C e roda jogos de SNES no **PiSNES** (SNES9X 1.39, vídeo GLES2/dispmanx). Nada foi testado em hardware ainda.

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

> Atualize esta seção ao fim de cada sessão. Última atualização: **2026-11-07**.

- **Versão publicada:** `v0.1.0` (Release em pré-lançamento, criada pela tag). Download também pelo site.
- **Hardware:** **nada testado ainda.** O dono vai gravar a `v0.1.0` manualmente num cartão SD de 32 GB e testar num Pi 1 Model B rev 2 com controle de Xbox 360.
- **Pronto (testado só fora do Pi):** imagem compila no CI; PiSNES compila e linka; launcher com lista, capas, favoritos/recentes, aviso de chip e telas de erro; jogos de teste na partição ROMS; teste de boot no QEMU no CI; página pública no ar.
- **Maior risco em aberto:** a pilha gráfica legada (GLES2/dispmanx via `rpi-userland`) com o kernel 6.6 no Pi 1. Se o jogo não abrir ou a tela ficar preta, é o primeiro suspeito.

## Próximos passos (em ordem)

1. **Validar a `v0.1.0` no hardware**, com os jogos de teste da partição ROMS. Lista de verificação:
   - o launcher aparece na TV (boot, framebuffer 1280×720, montagem de `/roms`);
   - o controle navega no menu (A do Xbox confirma, B volta, X favorita, Back troca o filtro);
   - **Teste - Controles** acende os botões dos jogadores 1 e 2 no emulador;
   - **Teste - Breakout** tem imagem e som; Back + Start volta ao menu; o save aparece;
   - desligar na tomada e ligar de novo não corrompe a partição ROMS.
   Sem saída na TV, a depuração precisa de adaptador USB-serial (login em `ttyAMA0`, 115200).
2. Corrigir o que o teste mostrar. Tudo isso está em `docs/pendencias.md`, na seção "A testar no hardware".
3. **Modo rede** (especificado em `docs/launcher.md` e desenhado no canvas): programa `snespi-upload` em C, QR code (ex.: `qrcodegen`), tela no launcher acionada por Start, Ethernet + `udhcpc` só sob demanda.
4. Limpeza antes de uma versão estável: tirar o login na serial e o `hello_pi` da defconfig; avaliar `fsck.fat` no boot para a partição ROMS; um `snes9x.cfg` para controles genéricos.
5. Quando o hardware estiver validado: Releases deixam de ser pré-lançamento (ver **Releases**).

## Releases

Releases são **públicas** e viram o download da página do projeto. Só crie uma quando o dono pedir.

**Como funciona:** push de uma tag `v*` → o job `image` do `build.yml` compila a imagem e cria a Release com `snes-pi-os-<tag>.img.xz`, `snes-pi-os-<tag>.img.xz.sha256` e as notas de `.github/release-notes.md`. Push na `main` e "Run workflow" manual **não** criam Release (o manual só gera um artefato temporário de 14 dias).

**Passo a passo:**

1. Confirme com o dono a versão e se pode publicar.
2. Garanta a `main` verde (`gh run list --branch main --limit 3`) e os docs em dia.
3. Se precisar, ajuste `.github/release-notes.md` (texto genérico, vale para qualquer versão) e faça commit.
4. Crie e envie a tag anotada:
   ```sh
   git tag -a v0.2.0 -m "snes-pi-os v0.2.0"
   git push origin v0.2.0
   ```
5. Acompanhe o build da tag (`gh run list --limit 3`, ~25–30 min com cache) e confira a Release:
   ```sh
   gh release view v0.2.0
   ```
6. Atualize **Estado atual** neste arquivo.

**Versionamento:** `0.x.y` enquanto não houver teste completo no hardware. Novo recurso sobe o `x`; só correções sobem o `y`.

**Pré-lançamento:** o passo "Criar release" usa `--prerelease`. Quando o hardware for validado, tire o `--prerelease` do `build.yml`; aí o link `releases/latest` passa a funcionar e o botão do site pode apontar para ele.

**Release com problema:** não reaproveite a tag. Corrija e publique a próxima versão. Para apagar uma Release, só com autorização do dono:
```sh
gh release delete v0.2.0 --cleanup-tag
```

## Serviços e infraestrutura

| Serviço | Detalhes |
|---|---|
| GitHub | `geraldoantonio/snes-pi-os`, público. Actions grátis nos runners padrão. Cache de `dl/` e `ccache` (~1,4 GB) entre builds. |
| Secrets | `CLOUDFLARE_API_TOKEN` (modelo "Edit Cloudflare Workers", conta + zona `geraldojunior.dev`) e `CLOUDFLARE_ACCOUNT_ID`. Quem cria e troca o token é o dono. |
| Página | Worker `snespi-site` (só arquivos estáticos), domínio `snespi.geraldojunior.dev`. Publicada pelo `site.yml` a cada push que mude `site/`. Validar sem publicar: `cd site && npx wrangler@4.158.0 deploy --dry-run`. |
| Design | Canvas "Launcher snes-pi-os" no claude.ai (privado do dono): telas do launcher, modo rede e página de upload. As fontes do canvas ainda usam Pixelify nos títulos; o launcher real usa IBM Plex Mono. |
| Artefatos do CI | `snespi-sdcard-<sha>` (imagem) e `qemu-boot-<sha>` (`tela.png` + `serial.log`), 14 dias. |

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
| `.github/release-notes.md` | Texto das Releases (instruções de instalação) |
| `CLAUDE.md` | Só importa este arquivo, para o Claude Code carregá-lo sozinho |

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
