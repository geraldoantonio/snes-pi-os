Imagem do snes-pi-os para o **Raspberry Pi 1** (Model B de 512 MB; o B+ também deve funcionar).

> **Testada num Raspberry Pi 1 Model B:** os jogos abrem e rodam, com teclado ou até dois controles USB estilo Super Nintendo (testado com o Knup KP-3124). Som e saves funcionam; jogos com chips especiais ainda sem teste. O envio de jogos pela rede foi testado com um adaptador Wi-Fi USB Realtek RTL8188CUS; pelo cabo de rede, ainda sem teste. Se testar, conte como foi nas Issues.

## Como instalar

1. Baixe o arquivo `.img.xz` abaixo (o `.sha256` serve para conferir o download).
2. Grave no cartão SD com o [Raspberry Pi Imager](https://www.raspberrypi.com/software/) ("Usar personalizado") ou o [balenaEtcher](https://etcher.balena.io/). Os dois aceitam o `.xz` direto. Tudo no cartão será apagado.
3. Ligue o Pi com HDMI e um controle USB (ou teclado). No primeiro boot, a unidade **ROMS** cresce para ocupar o cartão inteiro; por isso ligue o Pi uma vez **antes** de copiar os jogos. O menu já traz dois jogos de teste gratuitos: **Teste - Controles** e **Teste - Breakout**.
4. Para colocar seus jogos, ponha o cartão no computador e copie as ROMs para a unidade **ROMS**. Capas opcionais vão na pasta `capas`, com o mesmo nome da ROM (`.jpg` ou `.png`).

## Enviar jogos pela rede

Dá para mandar jogos e capas sem tirar o cartão do Pi, com um cabo de rede ou um adaptador Wi-Fi USB:

1. No menu, aperte **Start** e escolha **Receber jogos**. Com adaptador Wi-Fi, a primeira vez pede a rede e a senha, digitada com o controle.
2. A TV mostra um endereço, um PIN e um código QR. No celular, aponte a câmera para o código; no computador, abra o endereço e digite o PIN. O aparelho precisa estar na mesma rede.
3. Escolha os arquivos na página. Cada um aparece na TV quando chega.
4. Aperte **B** para concluir: a rede é desligada e os jogos entram na lista.

A rede só fica ligada enquanto essa tela está aberta. Com adaptador Wi-Fi, o controle pode falhar por alguns segundos logo depois de ligar o Pi; se continuar instável, use um hub USB com fonte própria.

Instruções completas, controles e perguntas frequentes: https://snespi.geraldojunior.dev

Use apenas jogos que você tem o direito de usar. Os jogos de teste são exemplos do [OpenSNES](https://github.com/k0b3n4irb/opensnes) (licença MIT).
