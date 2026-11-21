Firmware do **controle arcade** do snes-pi-os para o **Raspberry Pi Pico (RP2040)**: liga alavanca e botões de fliperama ao Raspberry Pi por USB, como um controle estilo Super Nintendo. Um Pico atende dois jogadores.

Esta versão é só do firmware do controle. A imagem do sistema (cartão SD) tem versões próprias, na [página de versões](https://github.com/geraldoantonio/snes-pi-os/releases).

## Qual arquivo baixar

| Arquivo | Uso |
|---|---|
| `snespi-arcade-2p-….uf2` | painel de dois jogadores (ou de um, deixando os pinos do jogador 2 soltos) |
| `snespi-arcade-1p-….uf2` | painel de um jogador usado junto com outro controle USB |

O `.sha256` serve para conferir o download.

## Como gravar

1. Segure o botão **BOOTSEL** do Pico e ligue o cabo USB (de dados) no computador. Aparece a unidade `RPI-RP2`.
2. Copie o arquivo `.uf2` para essa unidade. O Pico reinicia sozinho, já como controle.
3. Ligue o Pico no Pi e abra o **Teste - Controles** no menu.

Esquema de ligação e detalhes: [docs/controle-arcade.md](https://github.com/geraldoantonio/snes-pi-os/blob/main/docs/controle-arcade.md).
