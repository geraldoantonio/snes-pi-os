#include "pad.h"

#include <string.h>

/* Bit de cada botão no relatório (numeração do controle estilo SNES) */
static const uint8_t button_bit[PAD_INPUTS] = {
	[PAD_X] = 0, [PAD_A] = 1, [PAD_B] = 2, [PAD_Y] = 3,
	[PAD_L] = 4, [PAD_R] = 5, [PAD_SELECT] = 8, [PAD_START] = 9,
};

void pad_init(pad_t *p)
{
	memset(p, 0, sizeof(*p));
}

uint16_t pad_update(pad_t *p, uint16_t raw)
{
	for (int i = 0; i < PAD_INPUTS; i++) {
		uint16_t bit = (uint16_t)(1u << i);
		if (p->lock[i] > 0) {
			p->lock[i]--;
		} else if ((raw ^ p->state) & bit) {
			p->state ^= bit;
			p->lock[i] = PAD_DEBOUNCE_MS;
		}
	}
	return p->state;
}

/* Direções opostas juntas (painel com botões no lugar da alavanca) = centro */
static uint8_t axis(int neg, int pos)
{
	if (neg == pos)
		return PAD_AXIS_CENTER;
	return neg ? PAD_AXIS_MIN : PAD_AXIS_MAX;
}

void pad_report(uint16_t state, pad_report_t *r)
{
	r->x = axis(state >> PAD_LEFT & 1, state >> PAD_RIGHT & 1);
	r->y = axis(state >> PAD_UP & 1, state >> PAD_DOWN & 1);
	r->buttons[0] = r->buttons[1] = 0;
	for (int i = PAD_A; i < PAD_INPUTS; i++)
		if (state >> i & 1)
			r->buttons[button_bit[i] / 8] |= (uint8_t)(1u << (button_bit[i] % 8));
}
