#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../third_party/stb_truetype.h"

#define CACHE_SLOTS 512		/* potência de 2 */

typedef struct {
	int cp;			/* 0 = vazio */
	int w, h, xoff, yoff, adv;
	unsigned char *bmp;
} glyph_t;

/* Arquivos de fonte carregados, compartilhados entre tamanhos */
typedef struct face {
	char path[256];
	unsigned char *data;
	stbtt_fontinfo info;
	int refs;
	struct face *next;
} face_t;

static face_t *faces;

struct font {
	face_t *face;
	float scale;
	int ascent, descent, line_gap;
	glyph_t cache[CACHE_SLOTS];
};

static face_t *face_get(const char *path)
{
	for (face_t *fc = faces; fc; fc = fc->next)
		if (strcmp(fc->path, path) == 0) {
			fc->refs++;
			return fc;
		}

	FILE *fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	fseek(fp, 0, SEEK_END);
	long n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	face_t *fc = calloc(1, sizeof(*fc));
	unsigned char *data = n > 0 ? malloc((size_t)n) : NULL;
	if (!fc || !data || fread(data, 1, (size_t)n, fp) != (size_t)n ||
	    !stbtt_InitFont(&fc->info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
		fclose(fp);
		free(data);
		free(fc);
		return NULL;
	}
	fclose(fp);
	snprintf(fc->path, sizeof(fc->path), "%s", path);
	fc->data = data;
	fc->refs = 1;
	fc->next = faces;
	faces = fc;
	return fc;
}

static void face_put(face_t *fc)
{
	if (--fc->refs > 0)
		return;
	for (face_t **pp = &faces; *pp; pp = &(*pp)->next)
		if (*pp == fc) {
			*pp = fc->next;
			break;
		}
	free(fc->data);
	free(fc);
}

font_t *font_load(const char *path, int px)
{
	face_t *fc = face_get(path);
	if (!fc)
		return NULL;
	font_t *f = calloc(1, sizeof(*f));
	if (!f) {
		face_put(fc);
		return NULL;
	}
	f->face = fc;
	f->scale = stbtt_ScaleForMappingEmToPixels(&fc->info, (float)px);
	int a, d, g;
	stbtt_GetFontVMetrics(&fc->info, &a, &d, &g);
	f->ascent = (int)(a * f->scale + 0.5f);
	f->descent = (int)(d * f->scale - 0.5f);
	f->line_gap = (int)(g * f->scale + 0.5f);
	return f;
}

void font_free(font_t *f)
{
	if (!f)
		return;
	for (int i = 0; i < CACHE_SLOTS; i++)
		if (f->cache[i].cp)
			stbtt_FreeBitmap(f->cache[i].bmp, NULL);
	face_put(f->face);
	free(f);
}

int font_ascent(const font_t *f)
{
	return f->ascent;
}

int font_line_height(const font_t *f)
{
	return f->ascent - f->descent;
}

static glyph_t *glyph(font_t *f, int cp)
{
	unsigned h = ((unsigned)cp * 2654435761u) & (CACHE_SLOTS - 1);
	for (int probe = 0; probe < CACHE_SLOTS; probe++) {
		glyph_t *g = &f->cache[(h + probe) & (CACHE_SLOTS - 1)];
		if (g->cp == cp)
			return g;
		if (g->cp == 0) {
			int adv, lsb;
			stbtt_GetCodepointHMetrics(&f->face->info, cp, &adv, &lsb);
			g->adv = (int)(adv * f->scale + 0.5f);
			g->bmp = stbtt_GetCodepointBitmap(&f->face->info, 0, f->scale, cp,
							  &g->w, &g->h, &g->xoff, &g->yoff);
			g->cp = cp;
			return g;
		}
	}
	return NULL;	/* cache cheio: não acontece com texto latino */
}

/* Decodifica um caractere UTF-8; avança *s */
static int utf8_next(const char **s)
{
	const unsigned char *p = (const unsigned char *)*s;
	int cp;
	if (p[0] < 0x80) {
		cp = p[0];
		*s += 1;
	} else if ((p[0] & 0xE0) == 0xC0 && p[1]) {
		cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
		*s += 2;
	} else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
		cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
		*s += 3;
	} else if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
		cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
		     ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
		*s += 4;
	} else {
		cp = '?';
		*s += 1;
	}
	return cp;
}

int text_width(font_t *f, const char *utf8, int ls)
{
	int w = 0, prev = 0;
	while (*utf8) {
		int cp = utf8_next(&utf8);
		glyph_t *g = glyph(f, cp);
		if (!g)
			break;
		if (prev)
			w += (int)(stbtt_GetCodepointKernAdvance(&f->face->info, prev, cp) * f->scale);
		w += g->adv + (*utf8 ? ls : 0);
		prev = cp;
	}
	return w;
}

static void draw_glyph(surface_t *s, const glyph_t *g, int x, int base, uint16_t c)
{
	for (int j = 0; j < g->h; j++) {
		int py = base + g->yoff + j;
		if (py < 0 || py >= s->h)
			continue;
		const unsigned char *row = g->bmp + (size_t)j * g->w;
		uint16_t *dst = s->px + (size_t)py * s->stride;
		for (int i = 0; i < g->w; i++) {
			int px = x + g->xoff + i;
			if (px >= 0 && px < s->w)
				gfx_blend_px(&dst[px], c, row[i]);
		}
	}
}

int text_draw(surface_t *s, font_t *f, int x, int y, uint16_t c,
	      const char *utf8, int ls)
{
	int x0 = x, prev = 0, base = y + f->ascent;
	while (*utf8) {
		int cp = utf8_next(&utf8);
		glyph_t *g = glyph(f, cp);
		if (!g)
			break;
		if (prev)
			x += (int)(stbtt_GetCodepointKernAdvance(&f->face->info, prev, cp) * f->scale);
		if (g->bmp)
			draw_glyph(s, g, x, base, c);
		x += g->adv + (*utf8 ? ls : 0);
		prev = cp;
	}
	return x - x0;
}

int text_draw_fit(surface_t *s, font_t *f, int x, int y, int maxw, uint16_t c,
		  const char *utf8)
{
	if (text_width(f, utf8, 0) <= maxw)
		return text_draw(s, f, x, y, c, utf8, 0);

	static const char ell[] = "\xE2\x80\xA6";	/* … */
	int ellw = text_width(f, ell, 0);
	char buf[512];
	size_t n = 0;
	const char *p = utf8;
	while (*p && n < sizeof(buf) - 8) {
		const char *q = p;
		utf8_next(&q);
		size_t len = (size_t)(q - p);
		memcpy(buf + n, p, len);
		buf[n + len] = 0;
		if (text_width(f, buf, 0) + ellw > maxw)
			break;
		n += len;
		p = q;
	}
	memcpy(buf + n, ell, sizeof(ell));
	return text_draw(s, f, x, y, c, buf, 0);
}

int text_draw_wrap(surface_t *s, font_t *f, int x, int y, int maxw, int line_h,
		   uint16_t c, const char *utf8)
{
	char line[512];
	int lines = 0;
	const char *p = utf8;
	while (*p) {
		/* acumula palavras enquanto couberem; a primeira sempre entra */
		const char *best = NULL, *w = p;
		for (;;) {
			const char *e = w;
			while (*e == ' ')
				e++;
			while (*e && *e != ' ' && *e != '\n')
				e++;
			size_t len = (size_t)(e - p);
			if (len >= sizeof(line))
				break;
			memcpy(line, p, len);
			line[len] = 0;
			if (best && text_width(f, line, 0) > maxw)
				break;
			best = e;
			if (!*e || *e == '\n')
				break;
			w = e;
		}
		if (!best) {	/* uma palavra maior que o buffer */
			size_t len = strlen(p);
			best = p + (len < sizeof(line) - 1 ? len : sizeof(line) - 1);
		}
		size_t n = (size_t)(best - p);
		memcpy(line, p, n);
		line[n] = 0;
		text_draw(s, f, x, y + lines * line_h, c, line, 0);
		lines++;
		p = best;
		while (*p == ' ')
			p++;
		if (*p == '\n')
			p++;
	}
	return lines;
}
