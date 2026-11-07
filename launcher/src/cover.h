/* Capas dos jogos: /roms/capas/<nome-da-rom>.jpg|png, com cache pequeno. */
#ifndef COVER_H
#define COVER_H

#include "gfx.h"

void cover_init(const char *covers_dir);
void cover_shutdown(void);

/* 1 se existe arquivo de capa para o jogo (só consulta o disco) */
int cover_has_file(const char *stem);

/* Capa já reduzida para caber em box_w x box_h, se estiver no cache */
const surface_t *cover_peek(const char *stem, int box_w, int box_h);

/* Decodifica (se preciso) e devolve a capa; NULL se não há ou falhou */
const surface_t *cover_get(const char *stem, int box_w, int box_h);

#endif
