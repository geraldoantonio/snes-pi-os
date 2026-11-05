# Hardware e limites

## Alvo

| Item | Valor |
|---|---|
| Placa | Raspberry Pi 1 Model B+ |
| SoC | Broadcom BCM2835 |
| CPU | ARM1176JZF-S (ARMv6, VFP, sem NEON) @ 700 MHz |
| RAM | 512 MB |
| Armazenamento | cartão microSD |
| Saída de vídeo | HDMI, composto |
| Áudio | HDMI, jack 3,5 mm |
| USB | 4 portas (gamepads) |

Por ser ARMv6, o Buildroot deve ser configurado para `arm1176jzf-s` com hard-float (o `raspberrypi_defconfig` já faz isso).

## Desempenho esperado

O PiSNES roda a maioria dos jogos em velocidade total no Pi 1, e o readme do projeto recomenda overclock para isso. Pontos do próprio readme:

- O efeito "phosphor" (pós-processamento) não roda a velocidade cheia no Pi 1. Manter desligado.
- A opção `InterpolatedSound=1` deixa o Pi 1 lento demais. Manter desligada.

O overclock (`arm_freq` no `config.txt`) ainda precisa ser testado nesta placa: estabilidade, temperatura e fonte de alimentação variam.

## Compatibilidade de jogos

O PiSNES é baseado no SNES9X 1.39, uma versão antiga. A página do projeto cita como **não funcionando**:

- Yoshi's Island
- Street Fighter Alpha 2
- Star Fox

Jogos com chips especiais (Super FX, SA-1 etc.) devem ser tratados como suspeitos até teste. Precisão de vídeo e áudio também é menor que a de emuladores recentes.

Se a compatibilidade for um problema, o plano B é um frontend libretro próprio com os cores `snes9x2002` ou `snes9x2005`. Ver [pendencias.md](pendencias.md).

## Memória

Com 512 MB e um sistema mínimo, a RAM não deve ser o gargalo. O gargalo é a CPU de 700 MHz.

## Fontes

- Página do PiSNES no SourceForge: https://sourceforge.net/projects/pisnes/
- Readme do PiSNES: https://sourceforge.net/projects/pisnes/files
