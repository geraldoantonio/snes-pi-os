/* Saída de áudio pelo ALSA (Pi), numa thread própria. */
#include "music.h"

#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_t thread;
static volatile int running;
static snd_pcm_t *pcm;
static mdec_t *dec;

static void *play_thread(void *arg)
{
	(void)arg;
	short buf[1024 * 2];
	while (running) {
		int n = mdec_read(dec, buf, 1024);
		if (n <= 0)
			break;
		snd_pcm_sframes_t w = snd_pcm_writei(pcm, buf, (snd_pcm_uframes_t)n);
		if (w < 0 && snd_pcm_recover(pcm, (int)w, 1) < 0)
			break;
	}
	return NULL;
}

int mout_start(const char *path)
{
	dec = mdec_open(path);
	if (!dec) {
		fprintf(stderr, "snespi-launcher: música %s não abriu\n", path);
		return -1;
	}
	if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 ||
	    snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
			       (unsigned)mdec_channels(dec), (unsigned)mdec_rate(dec),
			       1, 500000) < 0) {	/* 0,5 s de buffer: aguenta a UI ocupada */
		fprintf(stderr, "snespi-launcher: ALSA indisponível, menu sem música\n");
		if (pcm)
			snd_pcm_close(pcm);
		pcm = NULL;
		mdec_close(dec);
		dec = NULL;
		return -1;
	}
	running = 1;
	if (pthread_create(&thread, NULL, play_thread, NULL) != 0) {
		running = 0;
		snd_pcm_close(pcm);
		pcm = NULL;
		mdec_close(dec);
		dec = NULL;
		return -1;
	}
	return 0;
}

void mout_stop(void)
{
	if (!pcm)
		return;
	/* a thread confere "running" a cada bloco (~46 ms a 22 kHz) */
	running = 0;
	/*
	 * O áudio do Pi passa pela GPU (VCHIQ): se o driver travar, o writei
	 * pode não voltar nunca. Espera no máximo 2 s e, se for preciso,
	 * abandona a thread em vez de travar o launcher antes do jogo.
	 */
	struct timespec limit;
	clock_gettime(CLOCK_REALTIME, &limit);
	limit.tv_sec += 2;
	if (pthread_timedjoin_np(thread, NULL, &limit) != 0) {
		fprintf(stderr, "snespi-launcher: áudio sem resposta, música abandonada\n");
		pthread_detach(thread);
		pcm = NULL;	/* não fecha: a thread presa ainda o usa */
		dec = NULL;
		return;
	}
	snd_pcm_drop(pcm);
	snd_pcm_close(pcm);	/* libera o dispositivo para o emulador */
	pcm = NULL;
	mdec_close(dec);
	dec = NULL;
}
