#include "music.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define STB_VORBIS_NO_PUSHDATA_API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtautological-compare"
#pragma GCC diagnostic ignored "-Wunused-value"
#include "../third_party/stb_vorbis.c"
#pragma GCC diagnostic pop

/* ---- escolha do arquivo ---- */

static char roms[600], fallback[600];
static int playing;
static volatile int fade_out_ms;	/* pedido de fade-out, lido pelo decodificador */

static int exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

void music_init(const char *roms_dir, const char *default_path)
{
	snprintf(roms, sizeof(roms), "%s", roms_dir);
	snprintf(fallback, sizeof(fallback), "%s", default_path ? default_path : "");
}

void music_play(void)
{
	if (playing)
		return;
	fade_out_ms = 0;
	char p[700];
	snprintf(p, sizeof(p), "%s/.snespi/sem-musica", roms);
	if (exists(p))
		return;
	static const char *const user[] = { "musica.ogg", "musica.wav", "musica.OGG", "musica.WAV" };
	for (size_t i = 0; i < sizeof(user) / sizeof(user[0]); i++) {
		snprintf(p, sizeof(p), "%s/%s", roms, user[i]);
		if (exists(p) && mout_start(p) == 0) {
			playing = 1;
			return;
		}
	}
	if (fallback[0] && exists(fallback) && mout_start(fallback) == 0)
		playing = 1;
}

void music_stop(void)
{
	if (!playing)
		return;
	mout_stop();
	playing = 0;
}

void music_fade_out(int ms)
{
	fade_out_ms = ms > 0 ? ms : 1;
}

void music_shutdown(void)
{
	music_stop();
}

/* ---- decodificador ---- */

#define GAIN 0.35f		/* volume baixo: é música de fundo */
#define FADE_SECONDS 1.5f

struct mdec {
	int is_ogg;
	int rate, channels;
	stb_vorbis *ogg;
	FILE *wav;
	long data_start, data_len, data_pos;
	long fade_total, fade_done;	/* em quadros */
	long out_total, out_done;	/* fade-out, em quadros (0: sem) */
};

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char *p)
{
	return p[0] | (p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* WAV PCM de 16 bits, mono ou estéreo */
static int wav_open(mdec_t *d, const char *path)
{
	d->wav = fopen(path, "rb");
	if (!d->wav)
		return -1;
	unsigned char h[12];
	if (fread(h, 1, 12, d->wav) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
		return -1;
	int have_fmt = 0;
	for (;;) {
		unsigned char ch[8];
		if (fread(ch, 1, 8, d->wav) != 8)
			return -1;
		unsigned long len = rd32(ch + 4);
		if (!memcmp(ch, "fmt ", 4)) {
			unsigned char f[16];
			if (len < 16 || fread(f, 1, 16, d->wav) != 16)
				return -1;
			if (rd16(f) != 1 || rd16(f + 14) != 16)	/* PCM 16 bits */
				return -1;
			d->channels = (int)rd16(f + 2);
			d->rate = (int)rd32(f + 4);
			have_fmt = 1;
			if (fseek(d->wav, (long)(len - 16 + (len & 1)), SEEK_CUR) != 0)
				return -1;
		} else if (!memcmp(ch, "data", 4)) {
			if (!have_fmt || d->channels < 1 || d->channels > 2)
				return -1;
			d->data_start = ftell(d->wav);
			d->data_len = (long)len;
			d->data_pos = 0;
			return 0;
		} else if (fseek(d->wav, (long)(len + (len & 1)), SEEK_CUR) != 0) {
			return -1;
		}
	}
}

mdec_t *mdec_open(const char *path)
{
	mdec_t *d = calloc(1, sizeof(*d));
	if (!d)
		return NULL;
	const char *dot = strrchr(path, '.');
	d->is_ogg = dot && (!strcmp(dot, ".ogg") || !strcmp(dot, ".OGG"));
	if (d->is_ogg) {
		int err = 0;
		d->ogg = stb_vorbis_open_filename(path, &err, NULL);
		if (!d->ogg) {
			free(d);
			return NULL;
		}
		stb_vorbis_info info = stb_vorbis_get_info(d->ogg);
		d->rate = (int)info.sample_rate;
		d->channels = info.channels > 2 ? 2 : info.channels;
	} else if (wav_open(d, path) != 0) {
		mdec_close(d);
		return NULL;
	}
	if (d->rate <= 0 || d->channels <= 0) {
		mdec_close(d);
		return NULL;
	}
	d->fade_total = (long)(FADE_SECONDS * d->rate);
	return d;
}

int mdec_rate(const mdec_t *d) { return d->rate; }
int mdec_channels(const mdec_t *d) { return d->channels; }

static int raw_read(mdec_t *d, short *out, int frames)
{
	if (d->is_ogg) {
		int n = stb_vorbis_get_samples_short_interleaved(d->ogg, d->channels, out,
								  frames * d->channels);
		if (n == 0) {	/* fim: volta ao início */
			stb_vorbis_seek_start(d->ogg);
			n = stb_vorbis_get_samples_short_interleaved(d->ogg, d->channels, out,
								      frames * d->channels);
		}
		return n;
	}
	long frame_bytes = 2L * d->channels;
	if (d->data_pos >= d->data_len) {
		d->data_pos = 0;
		if (fseek(d->wav, d->data_start, SEEK_SET) != 0)
			return 0;
	}
	long want = (long)frames * frame_bytes, left = d->data_len - d->data_pos;
	if (want > left)
		want = left - left % frame_bytes;
	size_t got = fread(out, 1, (size_t)want, d->wav);
	d->data_pos += (long)got;
	if (got == 0)
		d->data_pos = d->data_len;	/* força voltar ao início na próxima */
	/* WAV é little-endian, como o ARM do Pi e o Mac: sem conversão */
	return (int)(got / (size_t)frame_bytes);
}

int mdec_read(mdec_t *d, short *out, int frames)
{
	int n = raw_read(d, out, frames);
	if (fade_out_ms && !d->out_total)
		d->out_total = (long)fade_out_ms * d->rate / 1000 + 1;
	for (int i = 0; i < n; i++) {
		float g = GAIN;
		if (d->fade_done < d->fade_total) {
			g *= (float)d->fade_done / (float)d->fade_total;
			d->fade_done++;
		}
		if (d->out_total) {
			g *= d->out_done < d->out_total ? 1.0f - (float)d->out_done / (float)d->out_total : 0.0f;
			d->out_done++;
		}
		for (int c = 0; c < d->channels; c++) {
			int v = (int)(out[i * d->channels + c] * g);
			out[i * d->channels + c] = (short)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
		}
	}
	return n;
}

void mdec_close(mdec_t *d)
{
	if (!d)
		return;
	if (d->ogg)
		stb_vorbis_close(d->ogg);
	if (d->wav)
		fclose(d->wav);
	free(d);
}
