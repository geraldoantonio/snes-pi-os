/* Sem saída de áudio (modo de captura): a música nunca toca. */
#include "music.h"

int mout_start(const char *path)
{
	(void)path;
	return -1;
}

void mout_stop(void) {}
