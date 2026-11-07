/*
 * snespi-launcher: frontend do snes-pi-os.
 *
 * Lista as ROMs de /roms, mostra detalhes e capa, e executa o PiSNES
 * (snes9x) para o jogo escolhido, voltando ao menu quando ele termina.
 *
 *   snespi-launcher [--roms DIR] [--emu CAMINHO] [--fonts DIR]
 *                   [--no-scanlines] [--list]
 *                   [--shot ARQ.png --screen NOME --select N]
 */
#include "cover.h"
#include "prefs.h"
#include "platform.h"
#include "rom.h"
#include "ui.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int shot_write_png(const surface_t *s, const char *path);

#define REPEAT_DELAY_MS 350
#define REPEAT_RATE_MS 70
#define COVER_DELAY_MS 150
#define WARN_GUARD_MS 500
#define STATUS_MS 6000
#define MOUNT_RETRY_S 10

static volatile sig_atomic_t quit_flag;

static void on_signal(int sig)
{
	(void)sig;
	quit_flag = 1;
}

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* /roms é um ponto de montagem? (dispositivo diferente do pai) */
static int is_mountpoint(const char *path)
{
	struct stat a, b;
	char parent[600];
	snprintf(parent, sizeof(parent), "%s/..", path);
	if (stat(path, &a) != 0 || stat(parent, &b) != 0)
		return 0;
	return a.st_dev != b.st_dev || a.st_ino == b.st_ino;
}

typedef struct {
	const char *roms_dir, *emu, *fonts_dir, *shot, *shot_screen, *shot_filter;
	int shot_select, scanlines, require_mount, list;
	char saves_dir[600], covers_dir[600];
} config_t;

static void usage(void)
{
	fprintf(stderr,
		"uso: snespi-launcher [--roms DIR] [--emu CAMINHO] [--fonts DIR] [--no-scanlines] [--list]\n"
		"                     [--shot ARQ.png --screen list|warn|launch|empty|nomount --select N\n"
		"                      --filter todos|favoritos|recentes]\n");
}

static int parse_args(config_t *c, int argc, char **argv)
{
	c->roms_dir = "/roms";
	c->emu = "/usr/share/pisnes/snes9x";
	c->fonts_dir = "/usr/share/snespi/fonts";
	c->scanlines = 1;
	c->shot_screen = "list";
	c->shot_filter = "todos";
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(a, "--roms") && v) { c->roms_dir = v; i++; }
		else if (!strcmp(a, "--emu") && v) { c->emu = v; i++; }
		else if (!strcmp(a, "--fonts") && v) { c->fonts_dir = v; i++; }
		else if (!strcmp(a, "--shot") && v) { c->shot = v; i++; }
		else if (!strcmp(a, "--screen") && v) { c->shot_screen = v; i++; }
		else if (!strcmp(a, "--select") && v) { c->shot_select = atoi(v); i++; }
		else if (!strcmp(a, "--filter") && v) { c->shot_filter = v; i++; }
		else if (!strcmp(a, "--no-scanlines")) c->scanlines = 0;
		else if (!strcmp(a, "--list")) c->list = 1;
		else { usage(); return -1; }
	}
	c->require_mount = strcmp(c->roms_dir, "/roms") == 0;
	/* o PiSNES grava em $HOME/.snes96_snapshots; o frontend usa HOME=/roms */
	const char *home = getenv("HOME");
	snprintf(c->saves_dir, sizeof(c->saves_dir), "%s/.snes96_snapshots",
		 home && *home ? home : c->roms_dir);
	snprintf(c->covers_dir, sizeof(c->covers_dir), "%s/capas", c->roms_dir);
	return 0;
}

/* Executa o emulador e espera; devolve o status de saída ou -1 */
static int run_game(const config_t *c, const rom_t *r)
{
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		/* caminho absoluto: o snes9x acha snes9x.cfg pela pasta de argv[0] */
		execl(c->emu, c->emu, r->path, (char *)NULL);
		_exit(127);
	}
	int st;
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return -1;
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
}

/* ---- lista filtrada (todos / favoritos / recentes) ---- */

typedef struct {
	int *idx;	/* índices em roms->items */
	int n;
} view_t;

static void apply_favs(rom_list_t *l)
{
	for (size_t i = 0; i < l->count; i++)
		l->items[i].fav = prefs_is_fav(l->items[i].stem);
}

static int rom_index(const rom_list_t *l, const char *stem)
{
	for (size_t i = 0; i < l->count; i++)
		if (strcmp(l->items[i].stem, stem) == 0)
			return (int)i;
	return -1;
}

static void view_build(view_t *v, const rom_list_t *l, filter_t f)
{
	free(v->idx);
	v->idx = malloc((l->count ? l->count : 1) * sizeof(int));
	v->n = 0;
	if (!v->idx)
		return;
	if (f == FILTER_RECENT) {	/* ordem de uso, do mais recente */
		for (int i = 0; i < prefs_recent_count(); i++) {
			int k = rom_index(l, prefs_recent(i));
			if (k >= 0)
				v->idx[v->n++] = k;
		}
		return;
	}
	for (size_t i = 0; i < l->count; i++)
		if (f == FILTER_ALL || l->items[i].fav)
			v->idx[v->n++] = (int)i;
}

/* Posição do jogo na lista filtrada (0 se não está nela) */
static int view_find(const view_t *v, const rom_list_t *l, const char *stem)
{
	for (int i = 0; i < v->n; i++)
		if (strcmp(l->items[v->idx[i]].stem, stem) == 0)
			return i;
	return 0;
}

static char view_letter(const view_t *v, const rom_list_t *l, int pos)
{
	return l->items[v->idx[pos]].letter;
}

/* Pula para o primeiro jogo da letra anterior/seguinte */
static int jump_letter(const view_t *v, const rom_list_t *l, int sel, int dir)
{
	char cur = view_letter(v, l, sel);
	int i = sel;
	if (dir > 0) {
		while (i < v->n && view_letter(v, l, i) == cur)
			i++;
		return i < v->n ? i : sel;
	}
	/* volta ao início do grupo; se já está nele, ao início do grupo anterior */
	while (i > 0 && view_letter(v, l, i - 1) == cur)
		i--;
	if (i == sel && i > 0) {
		char prev = view_letter(v, l, i - 1);
		while (i > 0 && view_letter(v, l, i - 1) == prev)
			i--;
	}
	return i;
}

static void view_to_state(ui_state_t *st, const view_t *v, filter_t f)
{
	st->view = v->idx;
	st->view_count = v->n;
	st->filter = f;
	if (st->sel >= v->n)
		st->sel = v->n ? v->n - 1 : 0;
}

static int shot_mode(config_t *c, rom_list_t *roms)
{
	static const struct { const char *name; screen_t scr; } map[] = {
		{ "list", SCR_LIST }, { "warn", SCR_WARN }, { "launch", SCR_LAUNCH },
		{ "empty", SCR_EMPTY }, { "nomount", SCR_NOMOUNT },
	};
	screen_t scr = SCR_LIST;
	int found = 0;
	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		if (!strcmp(map[i].name, c->shot_screen)) {
			scr = map[i].scr;
			found = 1;
		}
	if (!found) {
		usage();
		return 2;
	}
	if ((scr == SCR_LIST || scr == SCR_WARN || scr == SCR_LAUNCH) && roms->count == 0) {
		fprintf(stderr, "snespi-launcher: nenhuma ROM em %s\n", c->roms_dir);
		return 1;
	}
	filter_t f = FILTER_ALL;
	if (!strcmp(c->shot_filter, "favoritos"))
		f = FILTER_FAV;
	else if (!strcmp(c->shot_filter, "recentes"))
		f = FILTER_RECENT;
	view_t v = { 0 };
	view_build(&v, roms, f);

	surface_t *s = gfx_surface_new(UI_W, UI_H);
	ui_state_t st = {
		.roms = roms, .pads = 1, .load_cover = 1, .scanlines = c->scanlines,
		.retry_secs = MOUNT_RETRY_S,
	};
	st.sel = c->shot_select < 0 ? 0 : c->shot_select;
	view_to_state(&st, &v, f);
	if ((scr == SCR_WARN || scr == SCR_LAUNCH) && v.n == 0) {
		fprintf(stderr, "snespi-launcher: filtro vazio\n");
		free(v.idx);
		gfx_surface_free(s);
		return 1;
	}
	ui_draw(s, scr, &st);
	int rc = shot_write_png(s, c->shot) == 0 ? 0 : 1;
	gfx_surface_free(s);
	free(v.idx);
	return rc;
}

int main(int argc, char **argv)
{
	config_t cfg;
	memset(&cfg, 0, sizeof(cfg));
	if (parse_args(&cfg, argc, argv) != 0)
		return 2;
	if (ui_init(cfg.fonts_dir) != 0)
		return 1;
	cover_init(cfg.covers_dir);
	prefs_init(cfg.roms_dir);

	rom_list_t roms = { 0 };
	int mounted = !cfg.require_mount || is_mountpoint(cfg.roms_dir);
	if (mounted) {
		prefs_load();
		rom_scan(&roms, cfg.roms_dir, cfg.saves_dir);
		apply_favs(&roms);
	}

	if (cfg.list) {
		/* nome<TAB>mapa<TAB>chip<TAB>suportado<TAB>região<TAB>save<TAB>tamanho */
		for (size_t i = 0; i < roms.count; i++) {
			const rom_t *r = &roms.items[i];
			printf("%s\t%s\t%s\t%d\t%s\t%d\t%ld\n", r->stem,
			       r->valid_header ? (r->hirom ? "HiROM" : "LoROM") : "?",
			       r->chip == CHIP_NONE ? "-" : chip_name(r->chip),
			       chip_supported(r->chip), region_short(r->region), r->has_save, r->size);
		}
		rom_list_free(&roms);
		ui_shutdown();
		return mounted ? 0 : 1;
	}

	if (cfg.shot) {
		int rc = shot_mode(&cfg, &roms);
		rom_list_free(&roms);
		cover_shutdown();
		ui_shutdown();
		return rc;
	}

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	surface_t *screen = video_open(UI_W, UI_H);
	if (!screen) {
		ui_shutdown();
		return 1;
	}
	input_open();

	ui_state_t st = { .roms = &roms, .scanlines = cfg.scanlines, .retry_secs = MOUNT_RETRY_S };
	filter_t filter = FILTER_ALL;
	view_t view = { 0 };
	view_build(&view, &roms, filter);
	view_to_state(&st, &view, filter);
	screen_t scr = !mounted ? SCR_NOMOUNT : (roms.count ? SCR_LIST : SCR_EMPTY);
	int dirty = 1, held = IN_NONE;
	long long next_repeat = 0, cover_due = now_ms() + COVER_DELAY_MS, warn_since = 0;
	long long status_until = 0, next_retry = now_ms() + 1000;
	char status[160] = "";

	while (!quit_flag) {
		long long t = now_ms();

		/* partição ausente: tenta de novo a cada 10 s, com contagem na tela */
		if (scr == SCR_NOMOUNT && t >= next_retry) {
			next_retry = t + 1000;
			if (--st.retry_secs <= 0) {
				st.retry_secs = MOUNT_RETRY_S;
				if (is_mountpoint(cfg.roms_dir)) {
					prefs_load();
					rom_scan(&roms, cfg.roms_dir, cfg.saves_dir);
					apply_favs(&roms);
					view_build(&view, &roms, filter);
					view_to_state(&st, &view, filter);
					scr = roms.count ? SCR_LIST : SCR_EMPTY;
				}
			}
			dirty = 1;
		}
		if (status_until && t >= status_until) {
			status_until = 0;
			st.status = NULL;
			dirty = 1;
		}
		int pads = input_pad_count();
		if (pads != st.pads) {
			st.pads = pads;
			dirty = 1;
		}
		/* capa: só decodifica depois que a navegação para */
		if (scr == SCR_LIST && cover_due && t >= cover_due) {
			cover_due = 0;
			st.load_cover = 1;
			dirty = 1;
		}

		if (dirty) {
			ui_draw(screen, scr, &st);
			video_present();
			dirty = 0;
			st.load_cover = 0;
		}

		/* próximo prazo: repetição, capa ou relógio da tela de erro */
		long long due = t + 1000;
		if (held != IN_NONE && next_repeat < due)
			due = next_repeat;
		if (cover_due && cover_due < due)
			due = cover_due;
		if (status_until && status_until < due)
			due = status_until;
		int timeout = (int)(due - now_ms());
		if (timeout < 0)
			timeout = 0;

		in_event_t ev;
		int got = input_poll(&ev, timeout);
		int btn = IN_NONE;
		if (got) {
			if (ev.pressed) {
				btn = ev.button;
				if (btn == IN_UP || btn == IN_DOWN || btn == IN_LEFT || btn == IN_RIGHT) {
					held = btn;
					next_repeat = now_ms() + REPEAT_DELAY_MS;
				}
			} else if (ev.button == held) {
				held = IN_NONE;
			}
		} else if (held != IN_NONE && now_ms() >= next_repeat) {
			btn = held;
			next_repeat = now_ms() + REPEAT_RATE_MS;
		}
		if (btn == IN_NONE)
			continue;

		if (scr == SCR_LIST) {
			int n = view.n, sel = st.sel;
			if (btn == IN_SELECT) {	/* troca o filtro, mantendo o jogo se ele estiver no novo */
				char keep[256] = "";
				if (n)
					snprintf(keep, sizeof(keep), "%s", roms.items[view.idx[sel]].stem);
				filter = (filter_t)((filter + 1) % FILTER_COUNT);
				view_build(&view, &roms, filter);
				st.sel = keep[0] ? view_find(&view, &roms, keep) : 0;
				view_to_state(&st, &view, filter);
				cover_due = now_ms() + COVER_DELAY_MS;
				held = IN_NONE;
				dirty = 1;
				continue;
			}
			if (n == 0)
				continue;
			if (btn == IN_X) {	/* favoritar / desfavoritar */
				rom_t *r = &roms.items[view.idx[sel]];
				r->fav = prefs_toggle_fav(r->stem);
				if (filter == FILTER_FAV) {
					view_build(&view, &roms, filter);
					view_to_state(&st, &view, filter);
				}
				dirty = 1;
				continue;
			}
			switch (btn) {
			case IN_UP: sel = sel > 0 ? sel - 1 : n - 1; break;
			case IN_DOWN: sel = sel < n - 1 ? sel + 1 : 0; break;
			case IN_LEFT: sel = sel - UI_VISIBLE_ROWS < 0 ? 0 : sel - UI_VISIBLE_ROWS; break;
			case IN_RIGHT: sel = sel + UI_VISIBLE_ROWS >= n ? n - 1 : sel + UI_VISIBLE_ROWS; break;
			case IN_L:
				if (filter == FILTER_ALL)
					sel = jump_letter(&view, &roms, sel, -1);
				break;
			case IN_R:
				if (filter == FILTER_ALL)
					sel = jump_letter(&view, &roms, sel, +1);
				break;
			case IN_A:
				if (!chip_supported(roms.items[view.idx[sel]].chip)) {
					scr = SCR_WARN;
					warn_since = now_ms();
					held = IN_NONE;
					dirty = 1;
					continue;
				}
				scr = SCR_LAUNCH;
				break;
			default: break;
			}
			if (sel != st.sel) {
				st.sel = sel;
				cover_due = now_ms() + COVER_DELAY_MS;
				dirty = 1;
			}
		} else if (scr == SCR_WARN) {
			if (btn == IN_B) {
				scr = SCR_LIST;
				dirty = 1;
			} else if (btn == IN_A && now_ms() - warn_since >= WARN_GUARD_MS) {
				scr = SCR_LAUNCH;
			}
		}

		if (scr == SCR_LAUNCH) {
			rom_t *game = &roms.items[view.idx[st.sel]];
			char stem[256];
			snprintf(stem, sizeof(stem), "%s", game->stem);
			prefs_add_recent(stem);	/* antes de abrir: conta mesmo se o jogo travar */
			ui_draw(screen, SCR_LAUNCH, &st);
			video_present();

			input_close();
			video_suspend();
			int rc = run_game(&cfg, game);
			sync();	/* saves no FAT */
			video_resume();
			input_open();

			if (rc == 127 || rc < 0) {
				snprintf(status, sizeof(status), "N\xC3\xA3o foi poss\xC3\xADvel abrir o emulador");
				st.status = status;
				status_until = now_ms() + STATUS_MS;
			} else if (rc != 0) {
				snprintf(status, sizeof(status), "O emulador fechou com erro (c\xC3\xB3""digo %d)", rc);
				st.status = status;
				status_until = now_ms() + STATUS_MS;
			}
			/* saves novos e eventuais mudanças em /roms */
			if (!cfg.require_mount || is_mountpoint(cfg.roms_dir)) {
				rom_scan(&roms, cfg.roms_dir, cfg.saves_dir);
				apply_favs(&roms);
				view_build(&view, &roms, filter);
				st.sel = view_find(&view, &roms, stem);
				view_to_state(&st, &view, filter);
				scr = roms.count ? SCR_LIST : SCR_EMPTY;
			} else {
				scr = SCR_NOMOUNT;
			}
			held = IN_NONE;
			cover_due = now_ms() + COVER_DELAY_MS;
			dirty = 1;
		}
	}

	input_close();
	video_close();
	free(view.idx);
	rom_list_free(&roms);
	cover_shutdown();
	ui_shutdown();
	return 0;
}
