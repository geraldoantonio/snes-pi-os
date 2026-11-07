/* Interface de plataforma: vídeo e entrada. */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "gfx.h"

/* ---- vídeo ---- */

/* Abre a saída com a resolução lógica w x h; devolve a superfície de desenho */
surface_t *video_open(int w, int h);
void video_present(void);
/* Libera a tela para outro programa (o emulador) e a retoma depois */
void video_suspend(void);
int video_resume(void);
void video_close(void);

/* ---- entrada ---- */

enum {
	IN_NONE = 0,
	IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT,
	IN_A, IN_B, IN_X, IN_Y, IN_L, IN_R,
	IN_START, IN_SELECT,
	IN_COUNT
};

typedef struct {
	int button;	/* IN_* */
	int pressed;	/* 1 apertou, 0 soltou */
} in_event_t;

int input_open(void);
void input_close(void);
/* Espera até timeout_ms por um evento; devolve 1 se preencheu ev */
int input_poll(in_event_t *ev, int timeout_ms);
/* Número de controles (não teclados) conectados */
int input_pad_count(void);

#endif
