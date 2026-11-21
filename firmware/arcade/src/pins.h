/*
 * Pino (GPIO) de cada entrada, na ordem de pad.h: cima, baixo, esquerda,
 * direita, A, B, X, Y, L, R, Select, Start. Cada chave liga o pino ao GND
 * (pull-up interno, ativo em nível baixo). GP0 e GP1 ficam livres (UART).
 */
#ifndef PINS_H
#define PINS_H

#include "pad.h"

#ifndef ARCADE_PLAYERS
#define ARCADE_PLAYERS 2
#endif

static const unsigned char arcade_pins[2][PAD_INPUTS] = {
	{ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 },		/* jogador 1 */
	{ 14, 15, 16, 17, 18, 19, 20, 21, 22, 26, 27, 28 },	/* jogador 2 */
};

#endif
