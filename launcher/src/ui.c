#include "ui.h"

#include "cover.h"
#include "font.h"

#include <stdio.h>
#include <string.h>

/* ---- paleta (a mesma do canvas) ---- */
#define C_BG	 rgb(0x0E1116)
#define C_PANEL	 rgb(0x161B22)
#define C_LINE	 rgb(0x2A313C)
#define C_LINE2	 rgb(0x3A4350)
#define C_ROWLN	 rgb(0x1C222B)
#define C_TEXT	 rgb(0xE8E6E1)
#define C_TEXT2	 rgb(0xC9CDD3)
#define C_MUTED	 rgb(0x9AA3AE)
#define C_DIM	 rgb(0x78818D)
#define C_ACCENT rgb(0xF2A93B)
#define C_INK	 rgb(0x14100A)
#define C_BLUE	 rgb(0x6CB6FF)
#define C_SHADE	 rgb(0x080A0D)

/* ---- fontes ---- */
enum { PX_BOLD, MONO_REG, MONO_SEMI, FACE_COUNT };
static const char *const FACE_FILES[FACE_COUNT] = {
	"PixelifySans-Bold.ttf",
	"IBMPlexMono-Regular.ttf", "IBMPlexMono-SemiBold.ttf",
};

static char fonts_path[256];

#define MAX_FONTS 48
static struct { int face, px; font_t *f; } fonts[MAX_FONTS];
static int nfonts;

static font_t *F(int face, int px)
{
	for (int i = 0; i < nfonts; i++)
		if (fonts[i].face == face && fonts[i].px == px)
			return fonts[i].f;
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", fonts_path, FACE_FILES[face]);
	font_t *f = font_load(path, px);
	if (f && nfonts < MAX_FONTS) {
		fonts[nfonts].face = face;
		fonts[nfonts].px = px;
		fonts[nfonts].f = f;
		nfonts++;
	}
	return f;
}

int ui_init(const char *fonts_dir)
{
	snprintf(fonts_path, sizeof(fonts_path), "%s", fonts_dir);
	for (int i = 0; i < FACE_COUNT; i++)
		if (!F(i, 16)) {
			fprintf(stderr, "snespi-launcher: fonte %s/%s não abriu\n",
				fonts_dir, FACE_FILES[i]);
			return -1;
		}
	return 0;
}

void ui_shutdown(void)
{
	for (int i = 0; i < nfonts; i++)
		font_free(fonts[i].f);
	nfonts = 0;
}

/* ---- ajudantes de texto e componentes ---- */

static int tw(int face, int px, const char *t, int ls)
{
	font_t *f = F(face, px);
	return f ? text_width(f, t, ls) : 0;
}

static int lh(int face, int px)
{
	font_t *f = F(face, px);
	return f ? font_line_height(f) : px;
}

static int txt(surface_t *s, int face, int px, int x, int y, uint16_t c, const char *t, int ls)
{
	font_t *f = F(face, px);
	return f ? text_draw(s, f, x, y, c, t, ls) : 0;
}

/* texto centrado verticalmente em cy */
static int txt_mid(surface_t *s, int face, int px, int x, int cy, uint16_t c, const char *t, int ls)
{
	return txt(s, face, px, x, cy - lh(face, px) / 2, c, t, ls);
}

static int txt_fit(surface_t *s, int face, int px, int x, int cy, int maxw, uint16_t c, const char *t)
{
	font_t *f = F(face, px);
	return f ? text_draw_fit(s, f, x, cy - font_line_height(f) / 2, maxw, c, t) : 0;
}

static int txt_wrap(surface_t *s, int face, int px, int x, int y, int maxw, int line_h,
		    uint16_t c, const char *t)
{
	font_t *f = F(face, px);
	return f ? text_draw_wrap(s, f, x, y, maxw, line_h, c, t) : 0;
}

/* Etiqueta com borda: altura 30, padding 4x10; devolve a largura */
static int pill(surface_t *s, int x, int y, const pixmap_t *icon, const char *t,
		uint16_t border, uint16_t fg, int dashed)
{
	int w = 2 + 10 + tw(MONO_REG, 14, t, 0) + 10 + 2;
	if (icon)
		w += icon->w * 2 + 8;
	if (dashed)
		gfx_frame_dashed(s, x, y, w, 30, 2, 6, border);
	else
		gfx_frame(s, x, y, w, 30, 2, border);
	int cx = x + 12;
	if (icon) {
		gfx_pixmap(s, icon, cx, y + 15 - icon->h, 2, fg, fg);
		cx += icon->w * 2 + 8;
	}
	txt_mid(s, MONO_REG, 14, cx, y + 15, fg, t, 0);
	return w;
}

/* Tecla quadrada (L, R, SELECT...) com centro vertical em cy; devolve a largura */
static int keycap(surface_t *s, int x, int cy, const char *t)
{
	int w = tw(MONO_SEMI, 13, t, 0) + 16;
	if (w < 30)
		w = 30;
	gfx_frame(s, x, cy - 13, w, 26, 2, C_TEXT);
	txt_mid(s, MONO_SEMI, 13, x + (w - tw(MONO_SEMI, 13, t, 0)) / 2, cy, C_TEXT, t, 0);
	return w;
}

/* Botão redondo (A/B) de diâmetro 30 */
static void round_btn(surface_t *s, int cx, int cy, const char *t, int filled, uint16_t ink)
{
	if (filled)
		gfx_disc(s, cx, cy, 15, C_ACCENT);
	else
		gfx_ring(s, cx, cy, 15, 2, ink);
	int w = tw(MONO_SEMI, 15, t, 0);
	txt_mid(s, MONO_SEMI, 15, cx - w / 2, cy, filled ? C_INK : ink, t, 0);
}

static void logo(surface_t *s, int x, int cy)
{
	gfx_pixmap(s, &ICON_CART, x, cy - 16, 2, C_ACCENT, C_BG);
	txt_mid(s, PX_BOLD, 30, x + 46, cy, C_TEXT, "snes-pi-os", 1);
}

static void header(surface_t *s, const ui_state_t *st, int right)
{
	logo(s, 40, 36);
	gfx_fill(s, 0, 70, UI_W, 2, C_LINE);
	if (!right)
		return;

	/* direita para a esquerda: controle, depois contagem */
	int x = 1240;
	const char *pad = st->pads > 0 ? "Controle 1 conectado" : "Nenhum controle";
	x -= tw(MONO_REG, 16, pad, 0);
	txt_mid(s, MONO_REG, 16, x, 36, C_MUTED, pad, 0);
	x -= 10 + 26;
	gfx_pixmap(s, &ICON_PAD, x, 36 - 9, 2, st->pads > 0 ? C_BLUE : C_ACCENT, 0);

	char n[32];
	snprintf(n, sizeof(n), "%zu", st->roms ? st->roms->count : (size_t)0);
	const char *rest = " jogos em /roms";
	x -= 32 + tw(MONO_REG, 16, rest, 0);
	txt_mid(s, MONO_REG, 16, x, 36, C_MUTED, rest, 0);
	x -= tw(MONO_SEMI, 16, n, 0);
	txt_mid(s, MONO_SEMI, 16, x, 36, C_TEXT, n, 0);
}

static void scan_overlay(surface_t *s, const ui_state_t *st)
{
	if (st->scanlines)
		gfx_scanlines(s, 46);
}

/* ---- tela: lista ---- */

int ui_list_top(int sel, int count)
{
	int top = sel - 4;
	if (top > count - UI_VISIBLE_ROWS)
		top = count - UI_VISIBLE_ROWS;
	if (top < 0)
		top = 0;
	return top;
}

/* ROM selecionada, ou NULL se o filtro está vazio */
static const rom_t *cur_rom(const ui_state_t *st)
{
	if (st->view_count <= 0 || st->sel < 0 || st->sel >= st->view_count)
		return NULL;
	return &st->roms->items[st->view[st->sel]];
}

static void tabs(surface_t *s, const ui_state_t *st, int x, int cy)
{
	static const char *const names[FILTER_COUNT] = { "TODOS", "FAVORITOS", "RECENTES" };
	for (int i = 0; i < FILTER_COUNT; i++) {
		int w = tw(MONO_SEMI, 15, names[i], 2);
		int on = st->filter == (filter_t)i;
		txt_mid(s, MONO_SEMI, 15, x, cy, on ? C_ACCENT : C_MUTED, names[i], 2);
		if (on)
			gfx_fill(s, x, cy + 12, w, 2, C_ACCENT);
		x += w + 28;
	}
}

static void list_empty(surface_t *s, const ui_state_t *st, int x, int y)
{
	if (st->filter == FILTER_FAV) {
		gfx_pixmap(s, &ICON_STAR, x, y + 4, 3, C_ACCENT, 0);
		txt(s, MONO_SEMI, 20, x + 40, y, C_TEXT, "Nenhum favorito ainda", 0);
		txt(s, MONO_REG, 16, x + 40, y + 34, C_MUTED,
		    "Na lista TODOS, aperte X num jogo para marc\xC3\xA1-lo.", 0);
	} else {
		txt(s, MONO_SEMI, 20, x, y, C_TEXT, "Nenhum jogo aberto ainda", 0);
		txt(s, MONO_REG, 16, x, y + 34, C_MUTED,
		    "Os 5 \xC3\xBAltimos jogos abertos aparecem aqui.", 0);
	}
}

static void list_rows(surface_t *s, const ui_state_t *st)
{
	const rom_list_t *l = st->roms;
	const int x = 40, w = 664, y0 = 132;

	tabs(s, st, x, 96 + 18);
	char counter[32];
	snprintf(counter, sizeof(counter), "%02d / %02d", st->view_count ? st->sel + 1 : 0, st->view_count);
	txt_mid(s, MONO_REG, 14, x + w - tw(MONO_REG, 14, counter, 2), 96 + 18, C_MUTED, counter, 2);

	if (st->view_count == 0) {
		list_empty(s, st, x + 8, y0 + 28);
		return;
	}

	int top = ui_list_top(st->sel, st->view_count);
	for (int k = 0; k < UI_VISIBLE_ROWS && top + k < st->view_count; k++) {
		const rom_t *r = &l->items[st->view[top + k]];
		int y = y0 + k * 48, cy = y + 24;
		int sel = (top + k) == st->sel;
		int right = x + w - 16;

		if (sel)
			gfx_fill(s, x, y, w, 48, C_ACCENT);
		else
			gfx_fill(s, x, y + 47, w, 1, C_ROWLN);

		/* região (44 px, alinhada à direita) */
		const char *reg = region_short(r->region);
		txt_mid(s, sel ? MONO_SEMI : MONO_REG, 13, right - tw(MONO_REG, 13, reg, 0), cy,
			sel ? C_INK : C_MUTED, reg, 0);
		right -= 44 + 16;

		/* etiqueta do chip */
		if (r->chip != CHIP_NONE) {
			const char *cn = chip_name(r->chip);
			int bad = !chip_supported(r->chip);
			int face = (sel || bad) ? MONO_SEMI : MONO_REG;
			int cw = tw(face, 13, cn, 0) + 20;
			uint16_t bc = sel ? C_INK : (bad ? C_ACCENT : C_LINE2);
			uint16_t fc = sel ? C_INK : (bad ? C_ACCENT : C_MUTED);
			gfx_frame(s, right - cw, cy - 12, cw, 24, 2, bc);
			txt_mid(s, face, 13, right - cw + 10, cy, fc, cn, 0);
			right -= cw + 16;
		}

		if (sel)
			gfx_pixmap(s, &ICON_ARROW, x + 16, cy - 8, 2, C_INK, 0);
		int tx = x + 46;
		if (r->fav) {
			gfx_pixmap(s, &ICON_STAR, tx, cy - 9, 2, sel ? C_INK : C_ACCENT, 0);
			tx += 18 + 10;
		}
		txt_fit(s, sel ? MONO_SEMI : MONO_REG, 20, tx, cy, right - tx,
			sel ? C_INK : C_TEXT, r->stem);
	}
}

static void letter_rail(surface_t *s, const ui_state_t *st)
{
	const int x = 720, y0 = 136, y1 = 628, w = 28;
	int used[26] = { 0 };
	for (size_t i = 0; i < st->roms->count; i++) {
		char c = st->roms->items[i].letter;
		if (c >= 'A' && c <= 'Z')
			used[c - 'A'] = 1;
	}
	const rom_t *cr = cur_rom(st);
	char cur = cr ? cr->letter : 0;
	for (int i = 0; i < 26; i++) {
		char l[2] = { (char)('A' + i), 0 };
		int y = y0 + i * (y1 - y0 - 18) / 25;
		int face = used[i] ? MONO_SEMI : MONO_REG;
		int lx = x + (w - tw(face, 14, l, 0)) / 2;
		if (l[0] == cur) {
			gfx_fill(s, x, y, w, 18, C_ACCENT);
			txt_mid(s, MONO_SEMI, 14, lx, y + 9, C_INK, l, 0);
		} else {
			txt_mid(s, face, 14, lx, y + 9, used[i] ? C_TEXT : C_DIM, l, 0);
		}
	}
}

static void letter_tile(surface_t *s, int x, int y, int size, int border, int px, char letter)
{
	char l[2] = { letter, 0 };
	gfx_frame(s, x, y, size, size, border, C_ACCENT);
	int w = tw(MONO_SEMI, px, l, 0);
	txt_mid(s, MONO_SEMI, px, x + (size - w) / 2, y + size / 2, C_ACCENT, l, 0);
}

static void details(surface_t *s, const ui_state_t *st)
{
	const rom_t *r = cur_rom(st);
	const int ax = 780, ay = 96, aw = 460, ah = 536;
	gfx_fill(s, ax, ay, aw, ah, C_PANEL);
	gfx_frame(s, ax, ay, aw, ah, 2, C_LINE);
	const int ix = ax + 28, iw = aw - 56;
	int y = ay + 28;

	if (!r) {	/* filtro vazio */
		const pixmap_t *icon = st->filter == FILTER_FAV ? &ICON_STAR : &ICON_PAD;
		int sc = st->filter == FILTER_FAV ? 12 : 8;
		int iy = ay + 150;
		gfx_pixmap(s, icon, ax + (aw - icon->w * sc) / 2, iy, sc, C_LINE2, 0);
		const char *hint = "Select troca o filtro";
		txt_mid(s, MONO_REG, 16, ax + (aw - tw(MONO_REG, 16, hint, 0)) / 2,
			iy + icon->h * sc + 40, C_MUTED, hint, 0);
		return;
	}

	/* capa */
	const int bh = 254, iw2 = iw - 4, ih2 = bh - 4;
	gfx_fill(s, ix, y, iw, bh, C_BG);
	gfx_frame(s, ix, y, iw, bh, 2, C_LINE);
	const surface_t *cv = st->load_cover ? cover_get(r->stem, iw2, ih2)
					     : cover_peek(r->stem, iw2, ih2);
	if (cv) {
		gfx_blit(s, cv, ix + 2 + (iw2 - cv->w) / 2, y + 2 + (ih2 - cv->h) / 2);
	} else if (!cover_has_file(r->stem)) {
		int ty = y + (bh - (128 + 16 + 18)) / 2;
		letter_tile(s, ix + (iw - 128) / 2, ty, 128, 4, 88, r->letter);
		char hint[300];
		snprintf(hint, sizeof(hint), "capas/%s.jpg", r->stem);
		const char *pre = "Sem capa \xC2\xB7 ";
		int pw = tw(MONO_REG, 13, pre, 0);
		int hw = tw(MONO_REG, 13, hint, 0);
		if (pw + hw > iw - 16)
			hw = iw - 16 - pw;
		int hx = ix + (iw - pw - hw) / 2;
		txt_mid(s, MONO_REG, 13, hx, ty + 128 + 16 + 9, C_MUTED, pre, 0);
		txt_fit(s, MONO_REG, 13, hx + pw, ty + 128 + 16 + 9, hw, C_TEXT, hint);
	}
	/* com arquivo mas ainda não carregada: quadro vazio até a navegação parar */
	y += bh + 16;

	/* etiquetas */
	int px = ix;
	px += pill(s, px, y, NULL, region_long(r->region), C_LINE2, C_TEXT, 0) + 8;
	if (r->has_save)
		px += pill(s, px, y, &ICON_SAVE, "Save", C_BLUE, C_BLUE, 0) + 8;
	else
		px += pill(s, px, y, NULL, "Sem save", C_LINE2, C_MUTED, 1) + 8;
	if (r->fav)
		pill(s, px, y, &ICON_STAR, "Favorito", C_ACCENT, C_ACCENT, 0);
	y += 30 + 16;

	/*
	 * Nome e arquivo, sempre na mesma altura (74 px) para não invadir
	 * os campos de baixo:
	 *  - cabe numa linha a 28 px: nome + arquivo;
	 *  - cabe em 2 linhas a 22 px: só o nome (o arquivo some);
	 *  - maior que isso: 1 linha a 22 px cortada com "…" + arquivo.
	 */
	if (tw(MONO_SEMI, 28, r->stem, 0) <= iw) {
		txt(s, MONO_SEMI, 28, ix, y, C_TEXT, r->stem, 0);
		y += 34 + 6;
		txt_fit(s, MONO_REG, 14, ix, y + 9, iw, C_MUTED, r->file);
		y += 18 + 16;
	} else {
		surface_t *scratch = gfx_surface_new(1, 1);
		int lines = scratch ? txt_wrap(scratch, MONO_SEMI, 22, 0, -1000, iw, 27, 0, r->stem) : 3;
		gfx_surface_free(scratch);
		if (lines <= 2) {
			txt_wrap(s, MONO_SEMI, 22, ix, y, iw, 27, C_TEXT, r->stem);
			y += 2 * 27 + 20;
		} else {
			txt_fit(s, MONO_SEMI, 22, ix, y + 13, iw, C_TEXT, r->stem);
			y += 27 + 6;
			txt_fit(s, MONO_REG, 14, ix, y + 9, iw, C_MUTED, r->file);
			y += 18 + 23;
		}
	}

	/* tamanho e mapa */
	char size[32];
	if (r->size >= 1024 * 1024)
		snprintf(size, sizeof(size), "%.1f MB", r->size / (1024.0 * 1024.0));
	else
		snprintf(size, sizeof(size), "%ld KB", r->size / 1024);
	char *comma = strchr(size, '.');
	if (comma)
		*comma = ',';
	if (strstr(size, ",0 MB"))
		memmove(strstr(size, ",0 MB"), " MB", 4);
	const int colw = iw / 2;
	txt(s, MONO_REG, 12, ix, y, C_MUTED, "TAMANHO", 2);
	txt(s, MONO_REG, 15, ix, y + 18, C_TEXT, size, 0);
	txt(s, MONO_REG, 12, ix + colw, y, C_MUTED, "MAPA", 2);
	txt(s, MONO_REG, 15, ix + colw, y + 18, C_TEXT, r->valid_header ? (r->hirom ? "HiROM" : "LoROM") : "?", 0);

	/* faixa de compatibilidade, no rodapé do painel */
	const int bottom = ay + ah - 28;
	if (!chip_supported(r->chip)) {
		char msg[200];
		snprintf(msg, sizeof(msg), "%s n\xC3\xA3o \xC3\xA9 emulado. O jogo pode n\xC3\xA3o abrir ou travar.",
			 r->chip == CHIP_OTHER ? "Este chip" : chip_name(r->chip));
		int bh2 = 14 + 2 * 20 + 14;
		int by = bottom - bh2;
		gfx_frame(s, ix, by, iw, bh2, 2, C_ACCENT);
		gfx_pixmap(s, &ICON_WARN, ix + 16, by + 15, 1, C_ACCENT, C_PANEL);
		txt_wrap(s, MONO_REG, 14, ix + 16 + 18 + 12, by + 14, iw - 32 - 30, 20, C_TEXT, msg);
	} else {
		char msg[120];
		if (r->chip == CHIP_DSP)
			snprintf(msg, sizeof(msg), "Compat\xC3\xADvel \xC2\xB7 %s \xC3\xA9 emulado", chip_name(r->chip));
		else
			snprintf(msg, sizeof(msg), "Compat\xC3\xADvel com o PiSNES");
		int bh2 = 50, by = bottom - bh2;
		gfx_frame(s, ix, by, iw, bh2, 2, C_BLUE);
		gfx_pixmap(s, &ICON_CHECK, ix + 16, by + 25 - 8, 2, C_BLUE, 0);
		txt_mid(s, MONO_REG, 14, ix + 16 + 20 + 12, by + 25, C_TEXT, msg, 0);
	}
}

static void footer_list(surface_t *s, const ui_state_t *st)
{
	const int cy = 688;
	gfx_fill(s, 0, 656, UI_W, 2, C_LINE);
	int x = 40;
	gfx_pixmap(s, &ICON_DPAD, x, cy - 13, 2, C_TEXT, 0);
	x += 26 + 10;
	x += txt_mid(s, MONO_REG, 16, x, cy, C_MUTED, "Navegar", 0) + 36;
	if (st->filter == FILTER_ALL) {
		x += keycap(s, x, cy, "L") + 10;
		x += keycap(s, x, cy, "R") + 10;
		x += txt_mid(s, MONO_REG, 16, x, cy, C_MUTED, "Letra", 0) + 32;
	}
	round_btn(s, x + 15, cy, "A", 1, C_TEXT);
	x += 30 + 10;
	x += txt_mid(s, MONO_REG, 16, x, cy, C_TEXT, "Jogar", 0) + 32;
	round_btn(s, x + 15, cy, "X", 0, C_TEXT);
	x += 30 + 10;
	const rom_t *r = cur_rom(st);
	x += txt_mid(s, MONO_REG, 16, x, cy, C_MUTED, r && r->fav ? "Desfavoritar" : "Favoritar", 0) + 32;
	x += keycap(s, x, cy, "SELECT") + 10;
	txt_mid(s, MONO_REG, 16, x, cy, C_MUTED, "Filtro", 0);

	if (st->status) {
		int w = tw(MONO_SEMI, 16, st->status, 0);
		gfx_fill(s, 1240 - w - 16, 660, w + 16, 56, C_BG);
		txt_mid(s, MONO_SEMI, 16, 1240 - w, cy, C_ACCENT, st->status, 0);
		return;
	}
	const char *b = "Select+Start", *c = " sai do jogo";
	int rx = 1240 - tw(MONO_REG, 16, c, 0);
	txt_mid(s, MONO_REG, 16, rx, cy, C_MUTED, c, 0);
	rx -= tw(MONO_SEMI, 16, b, 0);
	txt_mid(s, MONO_SEMI, 16, rx, cy, C_TEXT, b, 0);
}

static void draw_list(surface_t *s, const ui_state_t *st)
{
	gfx_clear(s, C_BG);
	header(s, st, 1);
	list_rows(s, st);
	if (st->filter == FILTER_ALL && st->view_count > 0)
		letter_rail(s, st);
	details(s, st);
	footer_list(s, st);
}

/* ---- tela: aviso de chip ---- */

static void draw_warn(surface_t *s, const ui_state_t *st)
{
	const rom_t *r = cur_rom(st);
	if (!r)
		return;
	ui_state_t under = *st;
	under.status = NULL;
	draw_list(s, &under);
	gfx_blend_fill(s, 0, 0, UI_W, UI_H, C_SHADE, 204);

	char title[400], body[400];
	if (r->chip == CHIP_OTHER) {
		snprintf(title, sizeof(title), "%s usa um chip especial", r->stem);
		snprintf(body, sizeof(body), "O PiSNES (SNES9X 1.39) n\xC3\xA3o emula este chip. "
			 "O jogo pode n\xC3\xA3o abrir, travar ou rodar sem gr\xC3\xA1""ficos. "
			 "Se travar, segure Select + Start para voltar.");
	} else {
		snprintf(title, sizeof(title), "%s usa o chip %s", r->stem, chip_name(r->chip));
		snprintf(body, sizeof(body), "O PiSNES (SNES9X 1.39) n\xC3\xA3o emula o %s. "
			 "O jogo pode n\xC3\xA3o abrir, travar ou rodar sem gr\xC3\xA1""ficos. "
			 "Se travar, segure Select + Start para voltar.", chip_name(r->chip));
	}

	const int w = 680, pad_x = 44, pad_y = 40, iw = w - 2 * pad_x;
	/* mede a altura antes de desenhar */
	surface_t *scratch = gfx_surface_new(1, 1);
	int tl = scratch ? txt_wrap(scratch, MONO_SEMI, 32, 0, -1000, iw, 38, 0, title) : 2;
	int bl = scratch ? txt_wrap(scratch, MONO_REG, 18, 0, -1000, iw, 28, 0, body) : 3;
	gfx_surface_free(scratch);
	int h = pad_y + 32 + 22 + tl * 38 + 22 + bl * 28 + 30 + 60 + pad_y;
	int x = (UI_W - w) / 2, y = (UI_H - h) / 2;

	gfx_fill(s, x, y, w, h, C_PANEL);
	gfx_frame(s, x, y, w, h, 4, C_ACCENT);
	int ix = x + pad_x, cy = y + pad_y;
	gfx_pixmap(s, &ICON_WARN, ix, cy, 2, C_ACCENT, C_PANEL);
	txt_mid(s, MONO_SEMI, 18, ix + 36 + 14, cy + 16, C_ACCENT, "CHIP N\xC3\x83O SUPORTADO", 3);
	cy += 32 + 22;
	txt_wrap(s, MONO_SEMI, 32, ix, cy, iw, 38, C_TEXT, title);
	cy += tl * 38 + 22;
	txt_wrap(s, MONO_REG, 18, ix, cy, iw, 28, C_TEXT2, body);
	cy += bl * 28 + 30;

	/* botões: B volta (destacado), A tenta mesmo assim */
	int bw = (iw - 16) / 2;
	gfx_fill(s, ix, cy, bw, 60, C_ACCENT);
	const char *l1 = "Voltar \xC3\xA0 lista";
	int g1 = 30 + 12 + tw(MONO_SEMI, 18, l1, 0);
	int gx = ix + (bw - g1) / 2;
	gfx_ring(s, gx + 15, cy + 30, 15, 2, C_INK);
	txt_mid(s, MONO_SEMI, 14, gx + 15 - tw(MONO_SEMI, 14, "B", 0) / 2, cy + 30, C_INK, "B", 0);
	txt_mid(s, MONO_SEMI, 18, gx + 42, cy + 30, C_INK, l1, 0);

	int bx = ix + bw + 16;
	gfx_frame(s, bx, cy, bw, 60, 2, C_LINE2);
	const char *l2 = "Tentar mesmo assim";
	int g2 = 30 + 12 + tw(MONO_REG, 18, l2, 0);
	gx = bx + (bw - g2) / 2;
	gfx_ring(s, gx + 15, cy + 30, 15, 2, C_TEXT);
	txt_mid(s, MONO_SEMI, 14, gx + 15 - tw(MONO_SEMI, 14, "A", 0) / 2, cy + 30, C_TEXT, "A", 0);
	txt_mid(s, MONO_REG, 18, gx + 42, cy + 30, C_TEXT, l2, 0);
}

/* ---- tela: iniciando ---- */

static void draw_launch(surface_t *s, const ui_state_t *st)
{
	const rom_t *r = cur_rom(st);
	if (!r)
		return;
	gfx_clear(s, C_BG);

	const surface_t *cv = cover_get(r->stem, 336, 240);
	int art_h = cv ? 240 + 16 : 200;
	int h = art_h + 28 + 57 + 10 + 20 + 28 + 16 + 14 + 26 + 28 + 12 + 56;
	int y = (UI_H - h) / 2;

	if (cv) {
		int bw = cv->w + 16, bh = cv->h + 16;
		int bx = (UI_W - bw) / 2;
		gfx_fill(s, bx, y + (art_h - bh) / 2, bw, bh, C_PANEL);
		gfx_frame(s, bx, y + (art_h - bh) / 2, bw, bh, 2, C_LINE);
		gfx_blit(s, cv, bx + 8, y + (art_h - bh) / 2 + 8);
	} else {
		letter_tile(s, (UI_W - 200) / 2, y, 200, 6, 144, r->letter);
	}
	y += art_h + 28;

	font_t *ft = F(MONO_SEMI, 44);
	int tw1 = ft ? text_width(ft, r->stem, 0) : 0;
	if (tw1 > 1100)
		tw1 = 1100;
	txt_fit(s, MONO_SEMI, 44, (UI_W - tw1) / 2, y + 28, 1100, C_TEXT, r->stem);
	y += 57 + 10;

	char sub[400];
	snprintf(sub, sizeof(sub), "%s \xC2\xB7 %s \xC2\xB7 %s", r->file, region_short(r->region),
		 r->hirom ? "HiROM" : "LoROM");
	int sw = tw(MONO_REG, 16, sub, 0);
	if (sw > 1100)
		sw = 1100;
	txt_fit(s, MONO_REG, 16, (UI_W - sw) / 2, y + 10, 1100, C_MUTED, sub);
	y += 20 + 28;

	/* blocos */
	int bx = (UI_W - (12 * 24 + 11 * 8)) / 2;
	for (int i = 0; i < 12; i++)
		gfx_fill(s, bx + i * 32, y, 24, 16, i < 4 ? C_ACCENT : C_LINE);
	y += 16 + 14;
	const char *ini = "INICIANDO";
	txt_mid(s, MONO_SEMI, 18, (UI_W - tw(MONO_SEMI, 18, ini, 4)) / 2, y + 13, C_TEXT, ini, 4);
	y += 26 + 28 + 12;

	/* cartões */
	const int cw = 380, gap = 20;
	int cx = (UI_W - (2 * cw + gap)) / 2;
	gfx_fill(s, cx, y, cw, 56, C_PANEL);
	gfx_frame(s, cx, y, cw, 56, 2, C_LINE);
	int kx = cx + 20;
	kx += keycap(s, kx, y + 28, "SELECT") + 6;
	kx += keycap(s, kx, y + 28, "START") + 16;
	txt_mid(s, MONO_REG, 16, kx, y + 28, C_TEXT2, "voltam para a lista", 0);

	cx += cw + gap;
	gfx_fill(s, cx, y, cw, 56, C_PANEL);
	if (r->has_save) {
		gfx_frame(s, cx, y, cw, 56, 2, C_BLUE);
		gfx_pixmap(s, &ICON_SAVE, cx + 20, y + 28 - 7, 2, C_BLUE, 0);
		txt_mid(s, MONO_REG, 16, cx + 20 + 14 + 14, y + 28, C_TEXT2, "Save do jogo encontrado", 0);
	} else {
		gfx_frame(s, cx, y, cw, 56, 2, C_LINE);
		txt_mid(s, MONO_REG, 16, cx + 20, y + 28, C_MUTED, "Primeira vez: sem save ainda", 0);
	}
	scan_overlay(s, st);
}

/* ---- telas: sem jogos / sem partição ---- */

static int step(surface_t *s, int x, int y, int n, int w, const char *t)
{
	char num[4];
	snprintf(num, sizeof(num), "%d", n);
	gfx_fill(s, x, y, 34, 34, C_ACCENT);
	txt_mid(s, MONO_SEMI, 18, x + (34 - tw(MONO_SEMI, 18, num, 0)) / 2, y + 17, C_INK, num, 0);
	int lines = txt_wrap(s, MONO_REG, 18, x + 50, y + 17 - lh(MONO_REG, 18) / 2, w - 50, 25, C_TEXT, t);
	int h = lines * 25;
	return (h > 34 ? h : 34) + 14;
}

static void draw_empty(surface_t *s, const ui_state_t *st, int nomount)
{
	gfx_clear(s, C_BG);
	header(s, st, 0);

	const int ix = 96, iy = 72 + (UI_H - 72 - 320) / 2;
	gfx_pixmap(s, nomount ? &ICON_SDCARD_X : &ICON_SDCARD, ix, iy, 20, C_LINE2, C_ACCENT);

	const int x = ix + 280 + 88, w = 1184 - x;
	int y = 72 + 110;
	if (!nomount) {
		y += 30;
		txt(s, MONO_SEMI, 40, x, y, C_TEXT, "Nenhum jogo encontrado", 0);
		y += 53 + 22;
		y += txt_wrap(s, MONO_REG, 18, x, y, w, 28, C_TEXT2,
			      "A parti\xC3\xA7\xC3\xA3o ROMS do cart\xC3\xA3o est\xC3\xA1 vazia. Para adicionar jogos:") * 28 + 22;
		y += step(s, x, y, 1, w, "Desligue o aparelho e coloque o cart\xC3\xA3o SD no computador.");
		y += step(s, x, y, 2, w, "Abra a unidade ROMS e copie os arquivos .sfc, .smc, .fig ou .swc.");
		y += step(s, x, y, 3, w, "Opcional: crie a pasta capas com imagens .jpg ou .png de mesmo nome da ROM.");
		y += step(s, x, y, 4, w, "Ejete o cart\xC3\xA3o, coloque de volta no Pi e ligue.");
		txt(s, MONO_REG, 14, x, y + 8, C_MUTED, "Use apenas jogos que voc\xC3\xAA tem o direito de usar.", 0);
	} else {
		txt(s, MONO_SEMI, 18, x, y, C_ACCENT, "ERRO NO CART\xC3\x83O", 3);
		y += 26 + 22;
		txt(s, MONO_SEMI, 40, x, y, C_TEXT, "A parti\xC3\xA7\xC3\xA3o de jogos n\xC3\xA3o abriu", 0);
		y += 53 + 22;
		y += txt_wrap(s, MONO_REG, 18, x, y, w, 28, C_TEXT2,
			      "N\xC3\xA3o foi poss\xC3\xADvel montar a 3\xC2\xAA parti\xC3\xA7\xC3\xA3o (ROMS). Isso pode acontecer "
			      "se o aparelho foi desligado enquanto gravava um save, ou se a parti\xC3\xA7\xC3\xA3o foi apagada.") * 28 + 22;
		txt(s, MONO_SEMI, 17, x, y, C_ACCENT, "\xE2\x80\xBA", 0);
		y += txt_wrap(s, MONO_REG, 17, x + 24, y, w - 24, 24, C_TEXT,
			      "No computador, verifique a unidade ROMS (Windows: Verificar erros; macOS: "
			      "Utilit\xC3\xA1rio de Disco \xE2\x80\xBA Primeiros socorros).") * 24 + 12;
		txt(s, MONO_SEMI, 17, x, y, C_ACCENT, "\xE2\x80\xBA", 0);
		y += txt_wrap(s, MONO_REG, 17, x + 24, y, w - 24, 24, C_TEXT,
			      "Se a unidade n\xC3\xA3o aparecer, grave a imagem do snes-pi-os de novo.") * 24 + 22;
		char retry[80];
		snprintf(retry, sizeof(retry), "Nova tentativa em %d s \xC2\xB7 ou desligue o aparelho", st->retry_secs);
		txt(s, MONO_REG, 16, x, y, C_MUTED, retry, 0);
	}
	scan_overlay(s, st);
}

void ui_draw(surface_t *s, screen_t scr, const ui_state_t *st)
{
	switch (scr) {
	case SCR_LIST:
		draw_list(s, st);
		scan_overlay(s, st);
		break;
	case SCR_WARN:
		draw_warn(s, st);
		scan_overlay(s, st);
		break;
	case SCR_LAUNCH:
		draw_launch(s, st);
		break;
	case SCR_EMPTY:
		draw_empty(s, st, 0);
		break;
	case SCR_NOMOUNT:
		draw_empty(s, st, 1);
		break;
	}
}
