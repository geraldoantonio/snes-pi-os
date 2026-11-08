/* Monitor do emulador (CPU por thread, carga, kernel) no emulador.log. */
#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>

void monitor_start(pid_t pid, int logfd);
void monitor_tick(void);	/* grava uma amostra e faz fsync do log */
void monitor_stop(void);

#endif
