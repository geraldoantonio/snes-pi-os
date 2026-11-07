/* Teste do decodificador de música: lê N segundos e confere o básico.
 * Uso: music-decode-test ARQUIVO SEGUNDOS */
#include "../src/music.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc != 3)
		return 2;
	mdec_t *d = mdec_open(argv[1]);
	if (!d) {
		printf("FALHOU: não abriu %s\n", argv[1]);
		return 1;
	}
	int rate = mdec_rate(d), ch = mdec_channels(d);
	long want = (long)atof(argv[2]) * rate, got = 0;
	int peak = 0;
	short buf[4096 * 2];
	while (got < want) {
		int n = mdec_read(d, buf, 4096);
		if (n <= 0) {
			printf("FALHOU: leitura parou em %ld quadros\n", got);
			return 1;
		}
		for (int i = 0; i < n * ch; i++)
			if (abs(buf[i]) > peak)
				peak = abs(buf[i]);
		got += n;
	}
	mdec_close(d);
	printf("ok %s: %d Hz, %d canal(is), %ld quadros, pico %d (de 32767)\n",
	       argv[1], rate, ch, got, peak);
	/* volume baixo: o pico tem de ficar bem abaixo do máximo */
	return (peak > 0 && peak < 16000) ? 0 : 1;
}
