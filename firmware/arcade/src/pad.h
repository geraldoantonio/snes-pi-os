/*
 * Lógica do controle arcade, sem depender do pico-sdk (testada no computador
 * com "make test").
 *
 * O relatório USB imita um controle genérico estilo SNES (ex.: Knup KP-3124):
 * eixos X e Y de 8 bits e dez botões, com X = 0, A = 1, B = 2, Y = 3, L = 4,
 * R = 5, Select = 8 e Start = 9. É a numeração que o launcher e o snes9x.cfg
 * do snes-pi-os já usam, então o arcade funciona sem mudar nada no sistema.
 */
#ifndef PAD_H
#define PAD_H

#include <stdint.h>

enum {
	PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
	PAD_A, PAD_B, PAD_X, PAD_Y, PAD_L, PAD_R,
	PAD_SELECT, PAD_START,
	PAD_INPUTS
};

/* Tempo em que uma entrada fica travada depois de mudar (repique da chave) */
#define PAD_DEBOUNCE_MS 5

#define PAD_AXIS_MIN 0x00
#define PAD_AXIS_CENTER 0x7f
#define PAD_AXIS_MAX 0xff

typedef struct {
	uint16_t state;			/* bit PAD_x ligado = apertado */
	uint8_t lock[PAD_INPUTS];	/* ms que faltam para aceitar outra mudança */
} pad_t;

/* Relatório HID: 4 bytes, na ordem do descritor em usb_descriptors.c */
typedef struct {
	uint8_t x, y;
	uint8_t buttons[2];
} pad_report_t;

void pad_init(pad_t *p);

/*
 * Chamar uma vez por milissegundo com a leitura crua (bit PAD_x ligado =
 * apertado). Devolve o estado filtrado: a mudança vale na hora (sem atraso)
 * e as seguintes da mesma entrada são ignoradas por PAD_DEBOUNCE_MS.
 */
uint16_t pad_update(pad_t *p, uint16_t raw);

void pad_report(uint16_t state, pad_report_t *r);

#endif
