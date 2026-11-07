/*
 * Favoritos e últimos jogados, guardados em /roms/.snespi/ (a raiz é
 * somente leitura). Um nome de ROM (sem extensão) por linha.
 */
#ifndef PREFS_H
#define PREFS_H

#define PREFS_MAX_RECENT 5

void prefs_init(const char *roms_dir);
void prefs_load(void);

int prefs_is_fav(const char *stem);
/* Liga/desliga o favorito e grava; devolve o novo estado */
int prefs_toggle_fav(const char *stem);

/* Põe o jogo no topo dos recentes e grava */
void prefs_add_recent(const char *stem);
int prefs_recent_count(void);
const char *prefs_recent(int i);	/* 0 = mais recente */

#endif
