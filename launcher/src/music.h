/*
 * Música de fundo do menu, em loop e com volume baixo.
 *
 * Ordem de escolha: /roms/.snespi/sem-musica desliga tudo; senão
 * /roms/musica.ogg ou /roms/musica.wav; senão a música padrão da imagem.
 * Para antes de abrir um jogo (o emulador precisa do dispositivo de áudio).
 */
#ifndef MUSIC_H
#define MUSIC_H

void music_init(const char *roms_dir, const char *default_path);
/* Começa do início, se houver música e não estiver tocando */
void music_play(void);
void music_stop(void);
/* Abaixa o volume até o silêncio em ms (transição para o jogo) */
void music_fade_out(int ms);
void music_shutdown(void);

/* ---- decodificador (OGG/WAV, sempre em loop), usado pelas saídas ---- */

typedef struct mdec mdec_t;

mdec_t *mdec_open(const char *path);
int mdec_rate(const mdec_t *d);
int mdec_channels(const mdec_t *d);
/* Preenche frames quadros intercalados em 16 bits, já com volume e fade-in;
 * volta ao início no fim do arquivo. Devolve quadros lidos (0 em erro). */
int mdec_read(mdec_t *d, short *out, int frames);
void mdec_close(mdec_t *d);

/* ---- saída de áudio (uma por plataforma) ---- */

int mout_start(const char *path);	/* 0 se começou a tocar */
void mout_stop(void);

#endif
