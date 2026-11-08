Imagem do snes-pi-os para o **Raspberry Pi 1** (Model B de 512 MB; o B+ também deve funcionar).

> **Pré-lançamento:** testada num Raspberry Pi 1 Model B (os jogos abrem e rodam), mas ainda sem validação completa de controles, saves e jogos. Se testar, conte como foi nas Issues.

## Como instalar

1. Baixe o arquivo `.img.xz` abaixo (o `.sha256` serve para conferir o download).
2. Grave no cartão SD com o [Raspberry Pi Imager](https://www.raspberrypi.com/software/) ("Usar personalizado") ou o [balenaEtcher](https://etcher.balena.io/). Os dois aceitam o `.xz` direto. Tudo no cartão será apagado.
3. Ligue o Pi com HDMI e um controle USB (ou teclado). O menu já traz dois jogos de teste gratuitos: **Teste - Controles** e **Teste - Breakout**.
4. Para colocar seus jogos, ponha o cartão no computador e copie as ROMs para a unidade **ROMS**. Capas opcionais vão na pasta `capas`, com o mesmo nome da ROM (`.jpg` ou `.png`).

Instruções completas, controles e perguntas frequentes: https://snespi.geraldojunior.dev

Use apenas jogos que você tem o direito de usar. Os jogos de teste são exemplos do [OpenSNES](https://github.com/k0b3n4irb/opensnes) (licença MIT).
