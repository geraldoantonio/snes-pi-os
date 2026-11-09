# snespi-launcher

Frontend do snes-pi-os em C: lista as ROMs de `/roms`, mostra detalhes e capa, e executa o PiSNES (`snes9x`), voltando ao menu quando o jogo termina. O layout segue o canvas "Launcher snes-pi-os".

## Como funciona

- **Vídeo:** desenha em memória (RGB565, 1280×720) e copia para `/dev/fb0`. O framebuffer é pedido em 1280×720 a 16 bpp; com a pilha gráfica legada do Pi, a GPU escala para a TV. Se o driver recusar, a imagem fica centralizada.
- **Entrada:** evdev (`/dev/input/event*`), com controles conectados depois detectados a cada 2 s. Controles Xbox, PlayStation e similares: A do Xbox (botão de baixo) confirma, B volta, X favorita. Controles genéricos estilo SNES: botão da direita (A) confirma, o de baixo (B) volta. O PiSNES, no jogo, tem mapeamento próprio (padrão de Xbox 360, por posição: B do Xbox = A do SNES). Teclado: setas, Enter/`d` = A, Esc/`c` = B, PageUp/`a` = L, PageDown/`z` = R (as mesmas teclas de L/R no jogo).
- **ROMs:** `.sfc`, `.smc`, `.fig`, `.swc` na raiz de `/roms`, em ordem alfabética sem diferenciar acentos. Do cabeçalho do cartucho saem região, LoROM/HiROM e chip (mesma pontuação do SNES9X 1.39). Jogos com Super FX, SA-1, SDD-1, S-RTC, C4, SPC7110 ou outro chip especial mostram aviso antes de abrir.
- **Saves:** existe save se houver `$HOME/.snes96_snapshots/<nome>.srm` (o frontend usa `HOME=/roms`).
- **Capas:** `/roms/capas/<nome-da-rom>.jpg` ou `.png`. Só a do jogo selecionado é decodificada, 150 ms depois que a navegação para; as 6 últimas ficam em cache.
- **Jogo:** `fork` + `execl` de `/usr/share/pisnes/snes9x <rom>` por caminho absoluto (o snes9x acha o `snes9x.cfg` pela pasta de `argv[0]`). Na volta: `sync`, framebuffer reconfigurado (o SDL pode ter mudado o modo) e lista relida.
- **Partição ausente:** tela de erro com nova tentativa a cada 10 s.
- **Música do menu:** em loop, volume baixo (35%, com fade-in). Usa `/roms/musica.ogg` ou `/roms/musica.wav` se existir; senão a música padrão (`music/menu.wav`, instalada em `/usr/share/snespi/musica/`). `/roms/.snespi/sem-musica` desliga. Para antes de abrir o jogo (o PiSNES precisa do ALSA) e volta na volta ao menu. Saída por ALSA no Pi e SDL no `make run`; OGG decodificado pelo `stb_vorbis`. Para poupar CPU no Pi 1, prefira WAV ou OGG mono a 22 kHz.
- **Log do emulador:** a saída do PiSNES (asserts, erros de EGL/ALSA) e o código de saída vão para `/roms/.snespi/emulador.log` a cada jogo aberto (passando de 64 KB, vira `emulador.log.1`). **Depuração** (desligada por padrão): Start › Depuração cria o arquivo `/roms/.snespi/depurar`, e com ele o monitor anota CPU, threads e kernel a cada 5 s durante o jogo (até 256 KB por sessão). Sem controle conectado, o rodapé mostra as teclas do teclado.

## Controles

| Botão | Teclado | Lista | Aviso de chip |
|---|---|---|---|
| Cima/baixo | Setas | Jogo anterior/seguinte (segurar repete) | |
| Esquerda/direita | Setas | Página anterior/seguinte | |
| L / R | `a` / `z` ou PageUp/PageDown | Letra anterior/seguinte (só em Todos) | |
| A | Enter ou `d` | Jogar | Tentar mesmo assim (após 0,5 s) |
| B | Esc ou `c` | | Voltar à lista |
| X | `s` | Favoritar / desfavoritar | |
| Select | Tab | Filtro: Todos → Favoritos → Recentes | |
| Start | Espaço | Menu do sistema: Desligar, Reiniciar, Voltar | |
| Botão de desligar (GPIO3) | F12 no `make run` | Desliga na hora (também no meio do jogo) | |

No jogo, Select + Start no controle volta ao menu (fixo no PiSNES); no teclado, Esc.

No menu do sistema: cima/baixo escolhem, A confirma, B ou Start fecham. Desligar mostra a tela "Desligando", pisca o LED `snespi-status` (se houver) e chama o `poweroff` do BusyBox. Fora do Pi, só fecha o launcher.

## Favoritos e recentes

Guardados em `/roms/.snespi/favoritos.txt` e `recentes.txt` (um nome de ROM por linha), porque a raiz é somente leitura. Recentes são os 5 últimos jogos **abertos**, em ordem de uso: sem relógio no Pi, não há data. Cada gravação vai para um arquivo temporário com `fsync` e depois `rename`, para um desligamento na tomada não deixar o arquivo pela metade. Jogos apagados de `/roms` somem das listas sozinhos.

## Desenvolvimento fora do Pi

```sh
make run          # abre o launcher numa janela (precisa de SDL2: brew install sdl2)
make preview      # binário que só gera capturas (macOS ou Linux)
make test         # gera ROMs de teste e confere a leitura dos cabeçalhos
make shots        # capturas de todas as telas em build/shots/
```

As ROMs de teste (`tests/make-test-roms.py`) são arquivos falsos, só com o cabeçalho do cartucho preenchido.

No `make run`, "jogar" executa `tests/fake-emu.sh`, que espera 2 s, grava um save falso e sai: dá para testar o ciclo completo (aviso de chip, iniciando, volta ao menu, save aparecendo). `SNESPI_FAKE_RC=1 make run` simula o emulador saindo com erro. Fechar a janela encerra o launcher. Com um controle conectado ao computador, o SDL o reconhece com o mesmo mapeamento do Pi (A do Xbox confirma).

No Pi, o mesmo binário aceita `--list` (o que ele leu de cada ROM) e `--shot arquivo.png --screen list|warn|launch|empty|nomount` para depurar.

## Licenças

- Código: MIT (`LICENSE`).
- `third_party/stb_*.h`: domínio público (Sean Barrett), commit `4e9b368b` de nothings/stb.
- Fontes (OFL 1.1): IBM Plex Mono (google/fonts `01d5d9f0`) e Pixelify Sans (eifetx/Pixelify-Sans `39df74ab`). Licenças em `fonts/`.
- Música padrão (`music/menu.wav`): "rpgchip01_title_screen" de Aureolus_Omicron, *15 Melodic RPG Chiptunes* no OpenGameArt, **CC0**. Convertida para WAV mono 22 kHz. Detalhes em `music/LICENCA.txt`.

A Pixelify Sans fica só no logo: no desenho dela, o "C" quase fechado parece "O", e o "B", o "Z" e alguns números lembram "8". Títulos e textos usam IBM Plex Mono.
