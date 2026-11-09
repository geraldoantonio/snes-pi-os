/* Primitivas de desenho em RGB565 (framebuffer do Pi 1 a 16 bpp). */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

typedef struct {
	int w, h;
	int stride;		/* em pixels */
	uint16_t *px;
	int owned;		/* 1 se px foi alocado por gfx_surface_new */
} surface_t;

/* 0xRRGGBB -> RGB565 */
static inline uint16_t rgb(uint32_t hex)
{
	return (uint16_t)(((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F));
}

surface_t *gfx_surface_new(int w, int h);
void gfx_surface_free(surface_t *s);

void gfx_clear(surface_t *s, uint16_t c);
void gfx_fill(surface_t *s, int x, int y, int w, int h, uint16_t c);
/* Borda de espessura t, para dentro do retângulo */
void gfx_frame(surface_t *s, int x, int y, int w, int h, int t, uint16_t c);
/* Borda tracejada (traços de d px) */
void gfx_frame_dashed(surface_t *s, int x, int y, int w, int h, int t, int d, uint16_t c);
/* Mistura c sobre a área com alfa 0..255 */
void gfx_blend_fill(surface_t *s, int x, int y, int w, int h, uint16_t c, int alpha);
void gfx_disc(surface_t *s, int cx, int cy, int r, uint16_t c);
void gfx_ring(surface_t *s, int cx, int cy, int r, int t, uint16_t c);
/* Escurece 1 de cada 3 linhas (efeito de scanline estático) */
void gfx_scanlines(surface_t *s, int alpha);
void gfx_blit(surface_t *dst, const surface_t *src, int x, int y);

/* Pinta um pixel com cobertura a (0..255) sobre o fundo */
static inline void gfx_blend_px(uint16_t *d, uint16_t c, int a)
{
	if (a <= 0)
		return;
	if (a >= 255) {
		*d = c;
		return;
	}
	unsigned s = *d, ia = 256 - (unsigned)a;
	unsigned r = (((c >> 11) & 31) * (unsigned)a + ((s >> 11) & 31) * ia) >> 8;
	unsigned g = (((c >> 5) & 63) * (unsigned)a + ((s >> 5) & 63) * ia) >> 8;
	unsigned b = ((c & 31) * (unsigned)a + (s & 31) * ia) >> 8;
	*d = (uint16_t)((r << 11) | (g << 5) | b);
}

/*
 * Ícones em pixel art: cada linha é uma string, '#' pinta com a cor
 * principal, '+' com a secundária, qualquer outro caractere é transparente.
 */
typedef struct {
	int w, h;
	const char *rows[24];
} pixmap_t;

void gfx_pixmap(surface_t *s, const pixmap_t *pm, int x, int y, int scale,
		uint16_t c1, uint16_t c2);

extern const pixmap_t ICON_CART;	/* logo: cartucho 16x16 */
extern const pixmap_t ICON_ARROW;	/* seta da linha selecionada 7x8 */
extern const pixmap_t ICON_WARN;	/* triângulo de aviso 18x16 */
extern const pixmap_t ICON_CHECK;	/* visto 10x8 */
extern const pixmap_t ICON_SAVE;	/* disquete 7x7 */
extern const pixmap_t ICON_DPAD;	/* direcional 13x13 */
extern const pixmap_t ICON_PAD;		/* controle 13x9 */
extern const pixmap_t ICON_STAR;	/* estrela (favorito) 9x9 */
extern const pixmap_t ICON_SDCARD;	/* cartão SD 14x16 */
extern const pixmap_t ICON_SDCARD_X;	/* cartão SD com X 14x16 */
extern const pixmap_t ICON_POWER;	/* liga/desliga 13x13 */

#endif
