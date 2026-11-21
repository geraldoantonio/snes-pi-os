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

## Implementação (primeira versão)

Código em [`launcher/`](../launcher/README.md), pacote `snes-launcher` no Buildroot. O `snespi-frontend` usa o launcher; um arquivo vazio `usar-gui-nativo` na partição ROMS volta para o GUI do PiSNES (depuração).

Feito: lista com capa e detalhes, aviso de chip, tela de iniciando, telas sem jogos e sem partição, execução do emulador e volta ao menu. Testado só fora do Pi: leitura de cabeçalhos com ROMs de teste e capturas de tela geradas no macOS. **Não testado no hardware** (framebuffer, controles, volta do emulador).

Depois da primeira versão:

- **Favoritos e recentes:** filtros Todos · Favoritos · Recentes (Select), X favorita. Gravados em `/roms/.snespi/`. Recentes = 5 últimos abertos, por ordem (sem relógio).
- **Teclado USB:** funciona no launcher (setas, Enter = A, Esc = B, `a`/`z` = L/R, `s` = X, Tab = Select) e no PiSNES (remapeado no `snes9x.cfg` do overlay: A = `c`, B = `x`, X = `d`, Y = `s`, L = `a`, R = `z`, Start = Enter, Select = Espaço, Esc sai), só para o jogador 1. No PiSNES, a entrada do teclado vem pelo SDL no `tty1` **(verificar no hardware)**.
- **Modo janela (SDL2)** para testar no computador: `make run` em `launcher/`.
- **Música de fundo no menu:** loop com volume baixo. Padrão: faixa CC0 do OpenGameArt ("rpgchip01_title_screen", Aureolus_Omicron), em WAV mono 22 kHz. O usuário troca com `/roms/musica.ogg` ou `.wav` e desliga com `/roms/.snespi/sem-musica`. Para antes do jogo e volta ao menu. Som no Pi ainda não validado **(verificar)**.
- **Transição para o jogo:** a tela "Iniciando" fica 3 s com a barra enchendo, enquanto a música do menu some (fade-out). Antes ela só piscava (resultado do teste no Pi). **(verificar no Pi** se a música some sem estalo**)**
- **Efeito de scanline desligado por padrão** (resultado do teste no Pi): na TV, a imagem de 720p é ampliada 1,5× para 1080p e as linhas escuras (1 a cada 3) ficam irregulares. Para ligar, crie o arquivo vazio `/roms/.snespi/scanlines` (ou use `--scanlines`).
- **Tela de abertura com carregamento:** logo (cartucho + "snes-pi-os") e uma barra em blocos, do boot até o menu. O launcher desenha e sai com `--splash PCT --splash-msg TEXTO`, deixando a imagem na tela e o `tty1` em modo gráfico, para o texto do console não aparecer por cima. Etapas: `S00splash` mostra 15 % ("Iniciando o sistema"), o `S99game` mostra 45 % ("Preparando som e controles") e o launcher vai de 60 % a 100 % enquanto lê as ROMs ("Lendo os jogos · N de M"). O framebuffer só é apagado quando o modo muda, por isso a abertura passa para o menu sem piscar preto. O `cmdline.txt` ganhou `logo.nologo`, que tira as framboesas do kernel. Capturas: `abertura*.png` em `make shots`. **(verificar no hardware:** quanto tempo cada chamada leva no Pi 1, já que carrega as fontes; se nenhum texto do boot aparece por cima)**.

- **Menu do sistema e desligamento:** Start (Espaço no teclado) abre o "Menu do sistema" sobre a lista, a tela sem jogos ou a de erro na partição: **Desligar**, **Reiniciar**, **Voltar**. O botão de desligar no GPIO (tecla `KEY_POWER`) desliga de qualquer tela, sem perguntar; no meio do jogo, o launcher manda `SIGTERM` ao PiSNES (que grava o save, patch `0007`), espera até 8 s (depois `SIGKILL`) e desliga. A tela "Desligando" fica até o fim (o launcher sai deixando a imagem, como na abertura) e o `poweroff`/`reboot` do BusyBox faz o resto. O LED `snespi-status` fica aceso no menu e pisca ao desligar. Fora do Pi, "desligar" só fecha o launcher (F12 simula o botão no `make run`). Capturas: `menu*.png`, `desligando.png`, `reiniciando.png`. **(verificar no hardware)**. O modo rede é o item **Receber jogos** deste menu (ver abaixo).
- **Depuração** (item do menu do sistema, desligada por padrão): liga e desliga o monitor do emulador, criando ou apagando o arquivo vazio `/roms/.snespi/depurar` (que também pode ser criado no PC). Sem ela, o `emulador.log` tem só o registro básico de cada jogo e o cartão é gravado uma vez aos 10 s e no fim; com ela, uma amostra a cada 5 s (cerca de 240 KB por hora, até 256 KB por sessão). Captura: `menu-depuracao.png`.

Diferenças em relação ao canvas:

- Títulos e textos em IBM Plex Mono; a Pixelify Sans ficou só no logo, porque no desenho dela "C" parece "O", e "B", "Z" e alguns números parecem "8".
- Sem o campo "Último jogo": o Pi não tem relógio, então a data dos saves não é confiável.
- O modo rede é um item do menu do sistema (Start), em vez da dica "Start · Receber pela rede" do canvas. As telas de rede foram feitas sem consultar o canvas (privado): conferir com o design. As de Wi-Fi (lista de redes e teclado da senha) não existem nele.
- Menu do sistema e tela "Desligando" não estão no canvas.

## Design

As telas estão no canvas "Launcher snes-pi-os" (1280×720, HDMI): lista de jogos com painel de detalhes e capa, aviso de chip não suportado, tela de carregamento e telas sem jogos ou sem partição.

- Dados por jogo saem do cabeçalho da ROM (título, região, mapa, chip) e de `/roms` (save em `.snes96_snapshots/<nome>.srm`).
- Capas opcionais em `/roms/capas/<nome-da-rom>.jpg|png`, fornecidas pelo usuário. Só a do jogo selecionado é decodificada (libjpeg ou `stb_image`), depois de ~150 ms parado, e as últimas 6 ficam em cache em RGB565. Sem capa, aparece a letra inicial.
- Jogos com Super FX, SA-1, C4, SDD-1, SPC7110 ou S-RTC (detectados pelo cabeçalho) mostram aviso antes de abrir, com o foco em "Voltar".

## Modo rede (receber jogos pela página web)

Além do cartão SD, o menu do sistema tem **Receber jogos** (Start › Receber jogos; na tela sem jogos, o menu já abre nesse item). Nada de rede fica ligado fora desse modo. Implementado em 2026-11-21; testado só no computador (`make run`, `make test`) **(verificar no hardware:** cabo, Wi-Fi, tempo do DHCP, velocidade e gravação no FAT**)**.

Fluxo:

1. O launcher chama `snespi-rede up` (script do overlay). Com o cabo ligado, sobe a `eth0` e roda o `udhcpc` do BusyBox. Sem cabo, usa o adaptador Wi-Fi USB com a rede de `/roms/wifi.txt`. A tela mostra "Conectando à rede" (B cancela).
2. Com o endereço em mãos, o launcher sorteia um PIN de 4 dígitos e inicia o `snespi-upload`. A tela mostra `http://192.168.x.x/1234`, o QR code do endereço e a conexão em uso. Sem o PIN no caminho, o servidor responde 404.
3. No celular ou no PC, a página deixa escolher ou arrastar arquivos e mostra o progresso de cada um. A tela do Pi lista os últimos arquivos recebidos e os recusados, com o motivo.
4. B conclui: o launcher para o servidor, chama `snespi-rede down`, faz `sync` e relê a lista (capas novas inclusive). O botão de desligar e o fim do launcher também derrubam a rede.

Se algo falha, a tela diz o motivo (sem cabo nem adaptador, Wi-Fi sem rede escolhida, senha errada, sem DHCP, servidor que não abriu) e oferece tentar de novo. O script copia o log para `/roms/.snespi/rede.log` em caso de erro.

**Wi-Fi USB:** o kernel já traz os drivers (`cfg80211`, `mac80211`, `rtl8xxxu`/`rtl8192cu`, `rt2800usb`, `mt7601u`, `ath9k_htc`). A imagem ganhou o `wpa_supplicant` (com `wpa_cli` e `wpa_passphrase`) e o firmware desses chips (`linux-firmware`: Realtek 81xx e 87xx, Ralink rt2xxx, MT7601U, Atheros 9271). Qual adaptador funciona no Pi 1: **(verificar)**. O `brcmfmac` USB ficou sem firmware (o pacote do Buildroot só traz os de SDIO e PCIe).

- Com adaptador presente, as telas de erro e de endereço oferecem "Escolher a rede Wi-Fi" (X): o launcher lista as redes (`snespi-rede scan`, até 8, da mais forte para a mais fraca, sem repetir nomes nem mostrar as ocultas) e abre um teclado na tela para a senha (A digita, B apaga, Y troca entre minúsculas, maiúsculas e símbolos, Start conecta, Select cancela). A senha aparece enquanto é digitada.
- A rede escolhida fica em `/roms/wifi.txt`: nome na 1ª linha, senha na 2ª (sem a 2ª, rede aberta). O arquivo também pode ser criado no computador. **A senha fica em texto puro no cartão.**
- O script passa o nome em hexadecimal e a chave já derivada (`wpa_passphrase`) ao `wpa_supplicant`, para aspas, espaços e acentos não quebrarem a configuração. Só WPA/WPA2 com senha e redes abertas; WPA3 puro não entra (exigiria OpenSSL na imagem).
- O cabo tem prioridade: com ele ligado, o Wi-Fi nem é tentado.

**Servidor de upload** (`snespi-upload`, `launcher/upload/`): programa próprio em C, separado do launcher e instalado pelo mesmo pacote. Serve a página embutida (`pagina.html`, que vira uma string em C no build) e recebe `multipart/form-data` em streaming direto para o disco. Cada conexão é atendida num processo filho, até 4 de uma vez: os navegadores abrem conexões de reserva e as deixam paradas, o que travaria um servidor de uma conexão só. Ele avisa o launcher de cada arquivo por linhas na saída padrão. O `httpd` do BusyBox não está na config padrão do Buildroot, e o tratamento do upload teria de ser escrito em C de qualquer forma.

Regras do upload:

- Extensões aceitas: `.sfc`, `.smc`, `.fig`, `.swc` para `/roms` (o PiSNES também abre `.zip`, mas o launcher ainda não os lista); `.jpg`, `.jpeg` e `.png` para `/roms/capas/`.
- Nome saneado: só a última parte do caminho, sem caracteres de controle nem os que o FAT recusa, sem ponto no começo, até 200 bytes. Um arquivo com o mesmo nome é substituído.
- Até 16 MB por arquivo.
- Grava em `/roms/.snespi/envio-<pid>.tmp` e renomeia no fim, com `fsync` do arquivo e da pasta, para não deixar ROM pela metade se cair a energia.
- Só funciona a partir do menu, nunca durante um jogo.
- Depois de 30 endereços com PIN errado, o servidor para de atender e a tela pede para abrir o modo de novo (PIN novo). O PIN protege contra envio por engano ou por curiosos na rede de casa; a conexão é HTTP, sem criptografia.

Testes no computador: `make test-upload` compila o servidor com ASan/UBSan e manda envios normais, nomes perigosos, corpos cortados e lixo; `make test-net` roda o modo rede sem tela, com uma rede falsa e o servidor de verdade. No `make run`, o item do menu funciona com `tests/fake-rede.sh` e o servidor na porta 8080 (`SNESPI_FAKE_WIFI=1 make run` finge um adaptador Wi-Fi; `SNESPI_FAKE_REDE=sem-rede` e os outros motivos simulam os erros). Capturas: `rede*.png` e `menu-rede.png`.

## Resolvido

- **Sair do jogo:** o PiSNES sai com Select+Start no controle 1 (fixo no código) ou Esc no teclado, e grava a SRAM antes de sair. O launcher só precisa esperar o processo terminar.
- **Mapeamento:** o `snes9x.cfg` do overlay é para controle USB genérico estilo SNES (Knup KP-3124: X=0, A=1, B=2, Y=3, L=4, R=5, Select=8, Start=9, direcional nos eixos 0 e 1), a mesma numeração que o launcher usa. O modelo do PiSNES era de Xbox 360 e tinha `QSAVE=9`, que nesses controles é o Start: segurar o Start gravava o estado sem parar e o jogo não começava. Com controle de Xbox, o Start (botão 7) não funciona no jogo; para os dois tipos, o launcher teria de escolher um `snes9x.cfg` por controle (ainda não feito).
