/* Texto com fontes TrueType (stb_truetype), rasterizadas uma vez por tamanho. */
#ifndef FONT_H
#define FONT_H

#include "gfx.h"

typedef struct font font_t;

/* px = tamanho em pixels (equivale ao font-size do CSS) */
font_t *font_load(const char *path, int px);
void font_free(font_t *f);

int font_ascent(const font_t *f);	/* da linha de base ao topo */
int font_line_height(const font_t *f);

/* Largura do texto UTF-8; letter_spacing em px entre caracteres */
int text_width(font_t *f, const char *utf8, int letter_spacing);

/*
 * Desenha com o topo da caixa de linha em y (como no CSS, line-height 1).
 * Devolve a largura desenhada.
 */
int text_draw(surface_t *s, font_t *f, int x, int y, uint16_t c,
	      const char *utf8, int letter_spacing);

/* Como text_draw, mas corta com "…" se passar de maxw */
int text_draw_fit(surface_t *s, font_t *f, int x, int y, int maxw, uint16_t c,
		  const char *utf8);

/* Quebra em linhas de até maxw; devolve o número de linhas desenhadas */
int text_draw_wrap(surface_t *s, font_t *f, int x, int y, int maxw, int line_h,
		   uint16_t c, const char *utf8);

#endif
