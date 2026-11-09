/*
 * Plataforma "nula" para desenvolvimento fora do Pi (macOS, Linux de PC):
 * desenha só em memória e não lê entrada. Usada com --shot para gerar
 * capturas das telas.
 */
#include "platform.h"

#include <unistd.h>

static surface_t *back;

surface_t *video_open(int w, int h)
{
	back = gfx_surface_new(w, h);
	return back;
}

void video_present(void) {}
void video_suspend(void) {}
int video_resume(void) { return 0; }

void video_close(void)
{
	gfx_surface_free(back);
	back = NULL;
}

void video_detach(void) { video_close(); }

int input_open(void) { return 0; }
void input_close(void) {}
int input_pad_count(void) { return 1; }

void power_watch_open(void) {}
int power_watch_pressed(void) { return 0; }
void power_watch_close(void) {}

/* fora do Pi não desliga nada: o launcher só fecha */
int system_power(int reboot) { (void)reboot; return 1; }
void system_led(int mode) { (void)mode; }

int input_poll(in_event_t *ev, int timeout_ms)
{
	(void)ev;
	if (timeout_ms > 0)
		usleep((useconds_t)timeout_ms * 1000);
	return 0;
}
