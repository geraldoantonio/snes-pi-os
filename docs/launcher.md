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

## Design

As telas estão no canvas "Launcher snes-pi-os" (1280×720, HDMI): lista de jogos com painel de detalhes e capa, aviso de chip não suportado, tela de carregamento e telas sem jogos ou sem partição.

- Dados por jogo saem do cabeçalho da ROM (título, região, mapa, chip) e de `/roms` (save em `.snes96_snapshots/<nome>.srm`).
- Capas opcionais em `/roms/capas/<nome-da-rom>.jpg|png`, fornecidas pelo usuário. Só a do jogo selecionado é decodificada (libjpeg ou `stb_image`), depois de ~150 ms parado, e as últimas 6 ficam em cache em RGB565. Sem capa, aparece a letra inicial.
- Jogos com Super FX, SA-1, C4, SDD-1, SPC7110 ou S-RTC (detectados pelo cabeçalho) mostram aviso antes de abrir, com o foco em "Voltar".

## Modo rede (receber jogos pela página web)

Decidido: além do cartão SD, o launcher terá uma opção **"Receber jogos pela rede"**. Nada de rede fica ligado fora desse modo.

Fluxo:

1. O usuário escolhe a opção no menu, com o cabo de rede ligado.
2. O launcher sobe a Ethernet (`ip link set eth0 up` + `udhcpc`, do BusyBox) e inicia o servidor de upload.
3. A tela mostra o endereço e um PIN de 4 dígitos sorteado na hora: `http://192.168.x.x/1234`. Sem o PIN no caminho, o servidor responde 404, para outro aparelho da rede não enviar arquivos por engano.
4. No celular ou no PC, a página permite arrastar ou escolher arquivos. A tela do Pi mostra cada arquivo recebido.
5. "Concluir" para o servidor, derruba a rede, faz `sync` e volta à lista, que é relida.

Servidor de upload: programa próprio em C (`snespi-upload`), separado do launcher. Ele atende uma conexão por vez, serve a página embutida e recebe `multipart/form-data` em streaming direto para o disco. O `httpd` do BusyBox não está na config padrão do Buildroot, e de qualquer forma o tratamento do upload teria de ser escrito em C.

Regras do upload:

- Extensões aceitas: `.sfc`, `.smc`, `.fig`, `.swc` (e `.zip` se o PiSNES abrir **(verificar)**) para `/roms`; `.jpg` e `.png` para `/roms/capas/`.
- Nome do arquivo saneado (sem `/`, `..` ou caracteres de controle) e tamanho máximo por arquivo (ex.: 16 MB).
- Grava em arquivo temporário e renomeia no fim, com `fsync`, para não deixar ROM pela metade se cair a energia.
- Só funciona a partir do menu, nunca durante um jogo.

## Resolvido

- **Sair do jogo:** o PiSNES sai com Select+Start no controle 1 (fixo no código) ou Esc no teclado, e grava a SRAM antes de sair. O launcher só precisa esperar o processo terminar.
- **Mapeamento:** o padrão do PiSNES é de controle de Xbox 360. Um gamepad USB genérico vai precisar de um `snes9x.cfg` próprio (códigos de botão do `jstest`).
