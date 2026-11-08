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

int input_poll(in_event_t *ev, int timeout_ms)
{
	(void)ev;
	if (timeout_ms > 0)
		usleep((useconds_t)timeout_ms * 1000);
	return 0;
}
