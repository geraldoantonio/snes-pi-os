/* Saída de áudio pelo SDL2 (modo janela no computador). */
#include "music.h"

#include <SDL.h>

static SDL_AudioDeviceID dev;
static mdec_t *dec;

static void callback(void *user, Uint8 *stream, int len)
{
	(void)user;
	int ch = mdec_channels(dec), frames = len / (2 * ch);
	short *out = (short *)stream;
	while (frames > 0) {
		int n = mdec_read(dec, out, frames);
		if (n <= 0) {
			SDL_memset(out, 0, (size_t)frames * 2 * ch);
			return;
		}
		out += n * ch;
		frames -= n;
	}
}

int mout_start(const char *path)
{
	if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		return -1;
	dec = mdec_open(path);
	if (!dec)
		return -1;
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = mdec_rate(dec);
	want.format = AUDIO_S16SYS;
	want.channels = (Uint8)mdec_channels(dec);
	want.samples = 2048;
	want.callback = callback;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!dev) {
		mdec_close(dec);
		dec = NULL;
		return -1;
	}
	SDL_PauseAudioDevice(dev, 0);
	return 0;
}

void mout_stop(void)
{
	if (!dev)
		return;
	SDL_CloseAudioDevice(dev);
	dev = 0;
	mdec_close(dec);
	dec = NULL;
}
