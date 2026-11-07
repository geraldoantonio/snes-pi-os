#include "gfx.h"

#include <stdlib.h>
#include <string.h>

surface_t *gfx_surface_new(int w, int h)
{
	surface_t *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;
	s->px = calloc((size_t)w * h, sizeof(uint16_t));
	if (!s->px) {
		free(s);
		return NULL;
	}
	s->w = w;
	s->h = h;
	s->stride = w;
	s->owned = 1;
	return s;
}

void gfx_surface_free(surface_t *s)
{
	if (!s)
		return;
	if (s->owned)
		free(s->px);
	free(s);
}

/* Recorta o retângulo à superfície; devolve 0 se ficou vazio */
static int clip(const surface_t *s, int *x, int *y, int *w, int *h)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > s->w) *w = s->w - *x;
	if (*y + *h > s->h) *h = s->h - *y;
	return *w > 0 && *h > 0;
}

void gfx_clear(surface_t *s, uint16_t c)
{
	gfx_fill(s, 0, 0, s->w, s->h, c);
}

void gfx_fill(surface_t *s, int x, int y, int w, int h, uint16_t c)
{
	if (!clip(s, &x, &y, &w, &h))
		return;
	for (int j = 0; j < h; j++) {
		uint16_t *p = s->px + (size_t)(y + j) * s->stride + x;
		for (int i = 0; i < w; i++)
			p[i] = c;
	}
}

void gfx_frame(surface_t *s, int x, int y, int w, int h, int t, uint16_t c)
{
	gfx_fill(s, x, y, w, t, c);
	gfx_fill(s, x, y + h - t, w, t, c);
	gfx_fill(s, x, y + t, t, h - 2 * t, c);
	gfx_fill(s, x + w - t, y + t, t, h - 2 * t, c);
}

void gfx_frame_dashed(surface_t *s, int x, int y, int w, int h, int t, int d, uint16_t c)
{
	for (int i = 0; i < w; i += 2 * d) {
		int n = (i + d > w) ? w - i : d;
		gfx_fill(s, x + i, y, n, t, c);
		gfx_fill(s, x + i, y + h - t, n, t, c);
	}
	for (int j = 0; j < h; j += 2 * d) {
		int n = (j + d > h) ? h - j : d;
		gfx_fill(s, x, y + j, t, n, c);
		gfx_fill(s, x + w - t, y + j, t, n, c);
	}
}

void gfx_blend_fill(surface_t *s, int x, int y, int w, int h, uint16_t c, int alpha)
{
	if (!clip(s, &x, &y, &w, &h))
		return;
	for (int j = 0; j < h; j++) {
		uint16_t *p = s->px + (size_t)(y + j) * s->stride + x;
		for (int i = 0; i < w; i++)
			gfx_blend_px(&p[i], c, alpha);
	}
}

void gfx_disc(surface_t *s, int cx, int cy, int r, uint16_t c)
{
	for (int dy = -r; dy <= r; dy++) {
		int dx = 0;
		while ((dx + 1) * (dx + 1) + dy * dy <= r * r)
			dx++;
		gfx_fill(s, cx - dx, cy + dy, 2 * dx + 1, 1, c);
	}
}

void gfx_ring(surface_t *s, int cx, int cy, int r, int t, uint16_t c)
{
	int ri = r - t;
	for (int dy = -r; dy <= r; dy++) {
		for (int dx = -r; dx <= r; dx++) {
			int d2 = dx * dx + dy * dy;
			if (d2 <= r * r && d2 > ri * ri) {
				int px = cx + dx, py = cy + dy;
				if (px >= 0 && py >= 0 && px < s->w && py < s->h)
					s->px[(size_t)py * s->stride + px] = c;
			}
		}
	}
}

void gfx_scanlines(surface_t *s, int alpha)
{
	for (int y = 2; y < s->h; y += 3)
		gfx_blend_fill(s, 0, y, s->w, 1, 0x0000, alpha);
}

void gfx_blit(surface_t *dst, const surface_t *src, int x, int y)
{
	int sx = 0, sy = 0, w = src->w, h = src->h;
	if (x < 0) { sx = -x; w += x; x = 0; }
	if (y < 0) { sy = -y; h += y; y = 0; }
	if (x + w > dst->w) w = dst->w - x;
	if (y + h > dst->h) h = dst->h - y;
	if (w <= 0 || h <= 0)
		return;
	for (int j = 0; j < h; j++)
		memcpy(dst->px + (size_t)(y + j) * dst->stride + x,
		       src->px + (size_t)(sy + j) * src->stride + sx,
		       (size_t)w * sizeof(uint16_t));
}

void gfx_pixmap(surface_t *s, const pixmap_t *pm, int x, int y, int scale,
		uint16_t c1, uint16_t c2)
{
	for (int j = 0; j < pm->h; j++) {
		const char *row = pm->rows[j];
		for (int i = 0; i < pm->w && row[i]; i++) {
			if (row[i] == '#')
				gfx_fill(s, x + i * scale, y + j * scale, scale, scale, c1);
			else if (row[i] == '+')
				gfx_fill(s, x + i * scale, y + j * scale, scale, scale, c2);
		}
	}
}

const pixmap_t ICON_CART = { 16, 16, {
	"................",
	"..############..",
	"..############..",
	"..##++++++++##..",
	"..##++++++++##..",
	"..##++++++++##..",
	"..##++++++++##..",
	"..##++++++++##..",
	"..############..",
	"..############..",
	"..############..",
	"..############..",
	"...##..##..##...",
	"...##..##..##...",
	"...##..##..##...",
	"................",
} };

const pixmap_t ICON_ARROW = { 7, 8, {
	"##.....",
	"####...",
	"######.",
	"#######",
	"#######",
	"######.",
	"####...",
	"##.....",
} };

const pixmap_t ICON_WARN = { 18, 16, {
	"........##........",
	"........##........",
	".......####.......",
	".......####.......",
	"......######......",
	"......##++##......",
	".....###++###.....",
	".....###++###.....",
	"....####++####....",
	"....####++####....",
	"...############...",
	"...#####++#####...",
	"..######++######..",
	"..##############..",
	".################.",
	".################.",
} };

const pixmap_t ICON_CHECK = { 10, 8, {
	"........##",
	".......##.",
	"......##..",
	"##...##...",
	".##.##....",
	"..###.....",
	"...#......",
	"..........",
} };

const pixmap_t ICON_SAVE = { 7, 7, {
	"######.",
	"#.##.##",
	"#.##..#",
	"#.....#",
	"#.....#",
	"#.....#",
	"#######",
} };

const pixmap_t ICON_DPAD = { 13, 13, {
	"....#####....",
	"....#####....",
	"....#####....",
	"....#####....",
	"#############",
	"#############",
	"#############",
	"#############",
	"#############",
	"....#####....",
	"....#####....",
	"....#####....",
	"....#####....",
} };

const pixmap_t ICON_PAD = { 13, 9, {
	"..#########..",
	".#.........#.",
	"#..#.....#..#",
	"#.###...#.#.#",
	"#..#.....#..#",
	"#...........#",
	"#...#####...#",
	".#.#.....#.#.",
	"..#.......#..",
} };

const pixmap_t ICON_SDCARD = { 14, 16, {
	"##########....",
	"#........#....",
	"#.+.+.+.+.#...",
	"#.+.+.+.+..#..",
	"#.+.+.+.+...#.",
	"#............#",
	"#............#",
	"#..########..#",
	"#..#......#..#",
	"#..#......#..#",
	"#..#......#..#",
	"#..########..#",
	"#............#",
	"#............#",
	"#............#",
	"##############",
} };

const pixmap_t ICON_SDCARD_X = { 14, 16, {
	"##########....",
	"#........#....",
	"#.+.+.+.+.#...",
	"#.+.+.+.+..#..",
	"#.+.+.+.+...#.",
	"#............#",
	"#............#",
	"#..########..#",
	"#..#+....+#..#",
	"#..#.+..+.#..#",
	"#..#..++..#..#",
	"#..#.+..+.#..#",
	"#..#+....+#..#",
	"#..########..#",
	"#............#",
	"##############",
} };
