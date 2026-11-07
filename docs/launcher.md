# Launcher (frontend mínimo)

## Objetivo

Um programa pequeno que substitui o GUI do PiSNES (`snes9x.gui`). Ele existe só para escolher uma ROM e iniciar o emulador, e voltar ao menu quando o jogo termina.

## Comportamento

1. Ao iniciar, varre `/roms` (e subpastas simples) por arquivos `.sfc`, `.smc`, `.fig`, `.swc`.
2. Mostra uma lista rolável na tela, em ordem alfabética.
3. Lê o gamepad: cima/baixo para navegar, A para iniciar.
4. Ao escolher uma ROM, executa `/usr/share/pisnes/snes9x /roms/<arquivo>` e espera terminar (`fork` + `execv` + `waitpid`). O caminho do binário precisa ser **absoluto**: o `snes9x` usa `argv[0]` para achar a própria pasta e o `snes9x.cfg`.
5. Quando o emulador sai, volta à lista.
6. Se não houver ROMs, mostra uma mensagem clara.

## Restrições de design

- Sem arquivo de configuração: tudo fixo em `#define`, recompilado quando mudar.
- Sem dependência de SDL próprio, se possível. A saída de texto pode ser feita com uma fonte bitmap embutida desenhada no framebuffer **(verificar)** se não conflitar com o dispmanx do PiSNES.
- Alternativa mais simples para a primeira versão: menu em texto no console (`/dev/tty1`), com leitura do gamepad via evdev. Evolui depois se necessário.
- Ao encerrar o emulador, o launcher deve garantir `sync()` para proteger os saves.

## Entrada

Leitura direta de `/dev/input/event*` (evdev). O mapeamento do gamepad é fixo no código; botões usados: direcional, A (confirmar), Start (opcional).

## Pontos em aberto

- Como o launcher e o PiSNES dividem a tela (modo texto vs. dispmanx) sem piscar entre eles.
- Se vale mais a pena manter o GUI nativo do PiSNES (`snes9x.gui`) na primeira versão e só trocar depois. O GUI executa `./snes9x "roms/<rom>"` a partir da própria pasta, então precisa de `roms/` (link para `/roms`) e `skins/` ao lado dele.

## Resolvido

- **Sair do jogo:** o PiSNES sai com Select+Start no controle 1 (fixo no código) ou Esc no teclado, e grava a SRAM antes de sair. O launcher só precisa esperar o processo terminar.
- **Mapeamento:** o padrão do PiSNES é de controle de Xbox 360. Um gamepad USB genérico vai precisar de um `snes9x.cfg` próprio (códigos de botão do `jstest`).
