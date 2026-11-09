# Hardware e limites

## Alvo

| Item | Valor |
|---|---|
| Placa | Raspberry Pi 1 Model B, revisão 2 (512 MB) |
| SoC | Broadcom BCM2835 |
| CPU | ARM1176JZF-S (ARMv6, VFP, sem NEON) @ 700 MHz |
| RAM | 512 MB |
| Armazenamento | cartão SD (tamanho padrão, não microSD) |
| Saída de vídeo | HDMI, composto (conector RCA amarelo) |
| Áudio | HDMI, jack 3,5 mm |
| USB | 2 portas |
| Rede | Ethernet 10/100 (sem Wi-Fi) |
| GPIO | 26 pinos |

Com só 2 portas USB, dois controles ocupam as duas. Para usar também um pendrive (importar ROMs), é preciso um hub USB, de preferência com fonte própria, porque as portas do Model B fornecem pouca corrente **(verificar com os seus acessórios)**.

O firmware escolhe o device tree pela revisão da placa (`bcm2708-rpi-b.dtb` no Model B). A defconfig gera também os DTBs do Model B rev 1 e do B+, então a mesma imagem deve servir nos três **(verificar)**.

Por ser ARMv6, o Buildroot deve ser configurado para `arm1176jzf-s` com hard-float (o `raspberrypi_defconfig` já faz isso).

## Botão de desligar e LED de status (GPIO, opcional)

Esquema: [`site/public/img/ligacao-gpio.svg`](../site/public/img/ligacao-gpio.svg) (também no README e no site).

| Peça | Pinos (conector P1, 26 pinos) | Configuração |
|---|---|---|
| Botão normalmente aberto | 5 (GPIO3) e 6 (GND) | `dtoverlay=gpio-shutdown,gpio_pin=3` (pull-up, ativo em nível baixo, tecla `KEY_POWER`) |
| LED + resistor de 330 Ω | 11 (GPIO17) → resistor → perna longa; perna curta → 9 (GND) | `dtoverlay=gpio-led,gpio=17,label=snespi-status,trigger=heartbeat` |

- **Botão:** o kernel (módulo `gpio_keys`, carregado pelo `S99game`) gera a tecla `KEY_POWER`, e o **launcher** desliga: no menu, na hora; no meio do jogo, manda `SIGTERM` ao PiSNES, que grava o save (patch `0007`) e sai, e então desliga. O desligamento é o `poweroff` do BusyBox, que roda os scripts de parada e desmonta `/roms`. Se o launcher não estiver rodando (ex.: GUI nativo), o botão não faz nada.
- **Ligar de novo:** com o Pi parado, encostar o GPIO3 no GND faz o firmware religar **(verificar no Pi 1)**. O botão não corta a energia; para isso, seria preciso um circuito externo (MOSFET ou módulo com trava) acionado pelo overlay `gpio-poweroff`.
- **LED:** pisca em batimento no boot (`heartbeat`), fica aceso no menu (o launcher troca para `default-on`), pisca rápido ao desligar (`timer`) e o driver `leds-gpio` o apaga no fim do desligamento: apagado, pode tirar da tomada **(verificar)**.
- O GPIO3 é também o SCL do I²C e tem pull-up de 1,8 kΩ na placa; nada usa I²C nesta imagem. No **Model B rev 1** o pino 5 é o GPIO1: troque para `gpio_pin=1`.
- Não use o GPIO14/15 (pinos 8 e 10): é a serial de depuração (`ttyAMA0`).
- **Reset sem GPIO:** a placa rev 2 tem os furos **P6** perto do HDMI; um botão soldado ali reinicia na hora (equivale a tirar da tomada: pode perder save). Não faz parte da imagem.
- Os overlays vão para a partição de boot com `BR2_PACKAGE_RPI_FIRMWARE_INSTALL_DTB_OVERLAYS=y` (pasta `overlays/`, ~1,8 MB).

## Desempenho esperado

O PiSNES roda a maioria dos jogos em velocidade total no Pi 1, e o readme do projeto recomenda overclock para isso. Pontos do próprio readme:

- O efeito "phosphor" (pós-processamento) não roda a velocidade cheia no Pi 1. Manter desligado.
- A opção `InterpolatedSound=1` deixa o Pi 1 lento demais. Manter desligada.

O readme do PiSNES usa `arm_freq=900` e `sdram_freq=500`, e a promessa de "maioria em velocidade total" depende disso. Ele também avisa para conferir se o governor da CPU não está em `powersave`. Em 700 MHz, sem overclock, o desempenho pode não bastar. O overclock ainda precisa ser testado nesta placa: estabilidade, temperatura e fonte de alimentação variam.

## Compatibilidade de jogos

O PiSNES é baseado no SNES9X 1.39, uma versão antiga. A página do projeto cita como **não funcionando**:

- Yoshi's Island
- Street Fighter Alpha 2
- Star Fox

### Chips especiais

O Makefile do PiSNES **não compila** os emuladores de chip `sa1`, `c4`, `fxemu` (Super FX), `sdd1`, `spc7110` e `srtc`, e as chamadas a eles parecem estar sob `#ifndef _ZAURUS` (o Makefile define `_ZAURUS`). Isso bate com a lista acima: Yoshi's Island e Star Fox são Super FX; Street Fighter Alpha 2 é SDD-1. O DSP-1 é compilado.

Por inferência, também **não devem funcionar** **(verificar)**:

| Chip | Exemplos |
|---|---|
| Super FX | Star Fox, Yoshi's Island, Stunt Race FX, Doom |
| SA-1 | Super Mario RPG, Kirby Super Star, Kirby's Dream Land 3 |
| C4 | Mega Man X2, Mega Man X3 |
| SDD-1 | Star Ocean, Street Fighter Alpha 2 |
| SPC7110 / S-RTC | Far East of Eden Zero, Daikaijuu Monogatari II |

Precisão de vídeo e áudio também é menor que a de emuladores recentes. Se esses jogos importarem, isso pesa a favor do plano B.

Se a compatibilidade for um problema, o plano B é um frontend libretro próprio com os cores `snes9x2002` ou `snes9x2005`. Ver [pendencias.md](pendencias.md).

## Memória

Com 512 MB e um sistema mínimo, a RAM não deve ser o gargalo. O gargalo é a CPU de 700 MHz.

## Fontes

- Página do PiSNES no SourceForge: https://sourceforge.net/projects/pisnes/
- Readme do PiSNES: https://sourceforge.net/projects/pisnes/files
