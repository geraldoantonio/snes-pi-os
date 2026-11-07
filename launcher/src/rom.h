/* Lista de ROMs e leitura do cabeçalho interno do cartucho. */
#ifndef ROM_H
#define ROM_H

#include <stddef.h>

typedef enum {
	CHIP_NONE = 0,
	CHIP_DSP,	/* DSP-1 (e variantes que o SNES9X 1.39 trata como DSP-1) */
	CHIP_SUPERFX,
	CHIP_SA1,
	CHIP_SDD1,
	CHIP_SRTC,
	CHIP_C4,
	CHIP_SPC7110,
	CHIP_OTHER,	/* tipo 0xFx desconhecido */
} chip_t;

typedef enum { REGION_UNKNOWN, REGION_JP, REGION_US, REGION_EU, REGION_OTHER_NTSC, REGION_OTHER_PAL } region_t;

typedef struct {
	char path[512];		/* caminho completo */
	char file[256];		/* nome do arquivo */
	char stem[256];		/* nome sem extensão (nome dos saves e capas) */
	char header_title[22];	/* título interno, sem espaços no fim */
	long size;		/* bytes, sem o cabeçalho de copiadora */
	int hirom;
	int valid_header;
	region_t region;
	chip_t chip;
	int has_save;
	int fav;		/* favorito (preenchido pelo main a partir de prefs) */
	char letter;		/* 'A'..'Z' ou '#' */
} rom_t;

typedef struct {
	rom_t *items;
	size_t count, cap;
} rom_list_t;

/*
 * Lê roms_dir (sem subpastas) e ordena por nome sem diferenciar
 * maiúsculas e acentos. saves_dir: onde ficam os .srm (pode ser NULL).
 * Devolve 0 em sucesso, -1 se a pasta não abriu.
 */
int rom_scan(rom_list_t *list, const char *roms_dir, const char *saves_dir);
void rom_list_free(rom_list_t *list);

/* Preenche os campos do cabeçalho a partir do arquivo; 0 em sucesso */
int rom_read_header(rom_t *r);

const char *chip_name(chip_t c);
int chip_supported(chip_t c);	/* 1 se o PiSNES emula */
const char *region_short(region_t r);	/* "NTSC" / "PAL" */
const char *region_long(region_t r);	/* "NTSC · América" */

/* Primeira letra de um texto UTF-8, sem acento, em maiúscula; '#' se não for letra */
char letter_of(const char *utf8);

#endif
