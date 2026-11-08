/* Desenho das telas do launcher (layout do canvas "Launcher snes-pi-os"). */
#ifndef SNESPI_UI_H
#define SNESPI_UI_H

#include "gfx.h"
#include "rom.h"

#define UI_W 1280
#define UI_H 720
#define UI_VISIBLE_ROWS 10

typedef enum {
	SCR_LIST,	/* lista de jogos */
	SCR_WARN,	/* aviso de chip não suportado (sobre a lista) */
	SCR_LAUNCH,	/* iniciando o jogo */
	SCR_EMPTY,	/* partição sem ROMs */
	SCR_NOMOUNT,	/* partição de ROMs não montou */
} screen_t;

typedef enum { FILTER_ALL, FILTER_FAV, FILTER_RECENT, FILTER_COUNT } filter_t;

typedef struct {
	const rom_list_t *roms;
	const int *view;	/* índices em roms->items mostrados pelo filtro */
	int view_count;
	filter_t filter;
	int sel;		/* posição selecionada em view */
	int pads;		/* controles conectados */
	int load_cover;		/* 1: pode decodificar a capa agora (fora da navegação rápida) */
	int scanlines;
	const char *status;	/* mensagem temporária no rodapé, ou NULL */
	int retry_secs;		/* contagem na tela SCR_NOMOUNT */
} ui_state_t;

int ui_init(const char *fonts_dir);
void ui_shutdown(void);
void ui_draw(surface_t *s, screen_t scr, const ui_state_t *st);

/*
 * Tela de abertura: logo e barra de carregamento (percent 0..100) com uma
 * mensagem da etapa. Usada no boot (--splash) e enquanto a lista é lida.
 */
void ui_draw_splash(surface_t *s, int percent, const char *msg, int scanlines);

/* Primeira linha visível da lista para a seleção atual */
int ui_list_top(int sel, int count);

#endif
