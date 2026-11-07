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
	const char *roms_dir, *emu, *fonts_dir, *shot, *shot_screen;
	int shot_select, scanlines, require_mount, list;
	char saves_dir[600], covers_dir[600];
} config_t;

static void usage(void)
{
	fprintf(stderr,
		"uso: snespi-launcher [--roms DIR] [--emu CAMINHO] [--fonts DIR] [--no-scanlines] [--list]\n"
		"                     [--shot ARQ.png --screen list|warn|launch|empty|nomount --select N]\n");
}

static int parse_args(config_t *c, int argc, char **argv)
{
	c->roms_dir = "/roms";
	c->emu = "/usr/share/pisnes/snes9x";
	c->fonts_dir = "/usr/share/snespi/fonts";
	c->scanlines = 1;
	c->shot_screen = "list";
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(a, "--roms") && v) { c->roms_dir = v; i++; }
		else if (!strcmp(a, "--emu") && v) { c->emu = v; i++; }
		else if (!strcmp(a, "--fonts") && v) { c->fonts_dir = v; i++; }
		else if (!strcmp(a, "--shot") && v) { c->shot = v; i++; }
		else if (!strcmp(a, "--screen") && v) { c->shot_screen = v; i++; }
		else if (!strcmp(a, "--select") && v) { c->shot_select = atoi(v); i++; }
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

static int find_stem(const rom_list_t *l, const char *stem)
{
	for (size_t i = 0; i < l->count; i++)
		if (strcmp(l->items[i].stem, stem) == 0)
			return (int)i;
	return 0;
}

/* Pula para o primeiro jogo da letra anterior/seguinte */
static int jump_letter(const rom_list_t *l, int sel, int dir)
{
	char cur = l->items[sel].letter;
	int i = sel;
	if (dir > 0) {
		while (i < (int)l->count && l->items[i].letter == cur)
			i++;
		return i < (int)l->count ? i : sel;
	}
	/* volta ao início do grupo; se já está nele, ao início do grupo anterior */
	while (i > 0 && l->items[i - 1].letter == cur)
		i--;
	if (i == sel && i > 0) {
		char prev = l->items[i - 1].letter;
		while (i > 0 && l->items[i - 1].letter == prev)
			i--;
	}
	return i;
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
	surface_t *s = gfx_surface_new(UI_W, UI_H);
	ui_state_t st = {
		.roms = roms, .pads = 1, .load_cover = 1, .scanlines = c->scanlines,
		.retry_secs = MOUNT_RETRY_S,
	};
	if (roms->count)
		st.sel = c->shot_select < 0 ? 0 :
			 (c->shot_select >= (int)roms->count ? (int)roms->count - 1 : c->shot_select);
	ui_draw(s, scr, &st);
	int rc = shot_write_png(s, c->shot) == 0 ? 0 : 1;
	gfx_surface_free(s);
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

	rom_list_t roms = { 0 };
	int mounted = !cfg.require_mount || is_mountpoint(cfg.roms_dir);
	if (mounted)
		rom_scan(&roms, cfg.roms_dir, cfg.saves_dir);

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
					rom_scan(&roms, cfg.roms_dir, cfg.saves_dir);
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
			int n = (int)roms.count, sel = st.sel;
			switch (btn) {
			case IN_UP: sel = sel > 0 ? sel - 1 : n - 1; break;
			case IN_DOWN: sel = sel < n - 1 ? sel + 1 : 0; break;
			case IN_LEFT: sel = sel - UI_VISIBLE_ROWS < 0 ? 0 : sel - UI_VISIBLE_ROWS; break;
			case IN_RIGHT: sel = sel + UI_VISIBLE_ROWS >= n ? n - 1 : sel + UI_VISIBLE_ROWS; break;
			case IN_L: sel = jump_letter(&roms, sel, -1); break;
			case IN_R: sel = jump_letter(&roms, sel, +1); break;
			case IN_A:
				if (!chip_supported(roms.items[sel].chip)) {
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
			char stem[256];
			snprintf(stem, sizeof(stem), "%s", roms.items[st.sel].stem);
			ui_draw(screen, SCR_LAUNCH, &st);
			video_present();

			input_close();
			video_suspend();
			int rc = run_game(&cfg, &roms.items[st.sel]);
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
				st.sel = roms.count ? find_stem(&roms, stem) : 0;
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
	rom_list_free(&roms);
	cover_shutdown();
	ui_shutdown();
	return 0;
}
