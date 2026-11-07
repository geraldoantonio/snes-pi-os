#include "rom.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *const EXTS[] = { ".sfc", ".smc", ".fig", ".swc", NULL };

static int has_rom_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	for (int i = 0; EXTS[i]; i++)
		if (strcasecmp(dot, EXTS[i]) == 0)
			return 1;
	return 0;
}

/* Letra base de um caractere Latin-1 acentuado (UTF-8 de 2 bytes C3 xx) */
static char fold_latin1(unsigned char b2)
{
	unsigned cp = 0xC0 | (b2 & 0x3F);
	if (cp >= 0xC0 && cp <= 0xC5) return 'A';
	if (cp == 0xC7) return 'C';
	if (cp >= 0xC8 && cp <= 0xCB) return 'E';
	if (cp >= 0xCC && cp <= 0xCF) return 'I';
	if (cp == 0xD1) return 'N';
	if (cp >= 0xD2 && cp <= 0xD6) return 'O';
	if (cp >= 0xD9 && cp <= 0xDC) return 'U';
	if (cp == 0xDD) return 'Y';
	if (cp >= 0xE0 && cp <= 0xE5) return 'a';
	if (cp == 0xE7) return 'c';
	if (cp >= 0xE8 && cp <= 0xEB) return 'e';
	if (cp >= 0xEC && cp <= 0xEF) return 'i';
	if (cp == 0xF1) return 'n';
	if (cp >= 0xF2 && cp <= 0xF6) return 'o';
	if (cp >= 0xF9 && cp <= 0xFC) return 'u';
	if (cp == 0xFD || cp == 0xFF) return 'y';
	return 0;
}

/* Próximo caractere comparável (minúsculo, sem acento); avança *s */
static int fold_next(const char **s)
{
	const unsigned char *p = (const unsigned char *)*s;
	if (p[0] == 0xC3 && p[1]) {
		char c = fold_latin1(p[1]);
		*s += 2;
		return c ? tolower((unsigned char)c) : 0x100 + p[1];
	}
	*s += 1;
	return tolower(p[0]);
}

static int fold_cmp(const char *a, const char *b)
{
	while (*a && *b) {
		int ca = fold_next(&a), cb = fold_next(&b);
		if (ca != cb)
			return ca - cb;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

static int cmp_rom(const void *x, const void *y)
{
	return fold_cmp(((const rom_t *)x)->stem, ((const rom_t *)y)->stem);
}

char letter_of(const char *utf8)
{
	const char *p = utf8;
	int c = fold_next(&p);
	if (c >= 'a' && c <= 'z')
		return (char)toupper(c);
	return '#';
}

/* ---- cabeçalho do cartucho (mesma pontuação do SNES9X 1.39) ---- */

static int all_ascii(const unsigned char *b, int n)
{
	for (int i = 0; i < n; i++)
		if (b[i] < 32 || b[i] > 126)
			return 0;
	return 1;
}

/* h = 0x50 bytes a partir de 0x?FB0 */
static int score(const unsigned char *h, long size, int hirom)
{
	int s = 0;
	if ((h[0x2c] + (h[0x2d] << 8) + h[0x2e] + (h[0x2f] << 8)) == 0xffff)
		s += 2;
	if (h[0x2a] == 0x33)
		s += 2;
	if ((h[0x25] & 0xf) < 4)
		s += 2;
	if (hirom) {
		if (size > 1024L * 1024 * 3)
			s += 4;
	} else if (size <= 1024L * 1024 * 16) {
		s += 2;
	}
	if (!(h[0x4d] & 0x80))	/* vetor de reset alto */
		s -= 4;
	if (h[0x27] >= 7 && (1 << (h[0x27] - 7)) > 48)
		s -= 1;
	if (!all_ascii(h, 6))
		s -= 1;
	if (!all_ascii(h + 0x10, 21))
		s -= 1;
	return s;
}

static int read_at(FILE *fp, long off, unsigned char *buf, size_t n)
{
	if (fseek(fp, off, SEEK_SET) != 0)
		return -1;
	return fread(buf, 1, n, fp) == n ? 0 : -1;
}

static chip_t classify(unsigned char speed, unsigned char type, int hirom, const char *title)
{
	unsigned hi = type & 0xf0, lo = type & 0x0f;
	unsigned map = speed & ~0x10u;

	if (!hirom && hi == 0x10)
		return CHIP_SUPERFX;
	if (map == 0x23 && hi == 0x30 && lo > 3)
		return CHIP_SA1;
	if (hi == 0x40)
		return CHIP_SDD1;
	if (hi == 0x50)
		return CHIP_SRTC;
	if (hi == 0xf0) {
		if (map == 0x3a)
			return CHIP_SPC7110;
		if (strncmp(title, "MEGAMAN X", 9) == 0 || strncmp(title, "ROCKMAN X", 9) == 0)
			return CHIP_C4;
		return CHIP_OTHER;
	}
	if (hi == 0x00 && lo >= 3)
		return CHIP_DSP;
	return CHIP_NONE;
}

static region_t region_of(unsigned char code)
{
	switch (code) {
	case 0: return REGION_JP;
	case 1: return REGION_US;
	case 2: return REGION_EU;
	default:
		/* SNES9X: 0, 1 e 13+ são NTSC; 2..12 são PAL */
		return (code >= 13) ? REGION_OTHER_NTSC : REGION_OTHER_PAL;
	}
}

int rom_read_header(rom_t *r)
{
	FILE *fp = fopen(r->path, "rb");
	if (!fp)
		return -1;
	fseek(fp, 0, SEEK_END);
	long total = ftell(fp);

	/* Cabeçalho de copiadora: 512 bytes a mais que um múltiplo de 8 KB */
	long skip = (total % 0x2000 == 512) ? 512 : 0;
	long size = total - skip;
	r->size = size;

	unsigned char lo[0x50], hi[0x50];
	int has_lo = size >= 0x8000 && read_at(fp, skip + 0x7fb0, lo, sizeof(lo)) == 0;
	int has_hi = size >= 0x10000 && read_at(fp, skip + 0xffb0, hi, sizeof(hi)) == 0;
	fclose(fp);
	if (!has_lo && !has_hi)
		return -1;

	int slo = has_lo ? score(lo, size, 0) : -100;
	int shi = has_hi ? score(hi, size, 1) : -100;
	r->hirom = shi > slo;
	const unsigned char *h = r->hirom ? hi : lo;

	memcpy(r->header_title, h + 0x10, 21);
	r->header_title[21] = 0;
	for (int i = 0; i < 21; i++)
		if ((unsigned char)r->header_title[i] < 32 || (unsigned char)r->header_title[i] > 126)
			r->header_title[i] = ' ';
	for (int i = 20; i >= 0 && r->header_title[i] == ' '; i--)
		r->header_title[i] = 0;

	r->chip = classify(h[0x25], h[0x26], r->hirom, r->header_title);
	r->region = region_of(h[0x29]);
	r->valid_header = (r->hirom ? shi : slo) > 0;
	return 0;
}

static int rom_push(rom_list_t *l, const rom_t *r)
{
	if (l->count == l->cap) {
		size_t ncap = l->cap ? l->cap * 2 : 64;
		rom_t *n = realloc(l->items, ncap * sizeof(*n));
		if (!n)
			return -1;
		l->items = n;
		l->cap = ncap;
	}
	l->items[l->count++] = *r;
	return 0;
}

int rom_scan(rom_list_t *list, const char *roms_dir, const char *saves_dir)
{
	list->count = 0;
	DIR *d = opendir(roms_dir);
	if (!d)
		return -1;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (e->d_name[0] == '.' || !has_rom_ext(e->d_name))
			continue;
		rom_t r;
		memset(&r, 0, sizeof(r));
		snprintf(r.path, sizeof(r.path), "%s/%s", roms_dir, e->d_name);
		struct stat st;
		if (stat(r.path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		snprintf(r.file, sizeof(r.file), "%s", e->d_name);
		snprintf(r.stem, sizeof(r.stem), "%s", e->d_name);
		char *dot = strrchr(r.stem, '.');
		if (dot)
			*dot = 0;
		r.letter = letter_of(r.stem);
		rom_read_header(&r);
		if (saves_dir) {
			char srm[800];
			snprintf(srm, sizeof(srm), "%s/%s.srm", saves_dir, r.stem);
			r.has_save = stat(srm, &st) == 0 && st.st_size > 0;
		}
		if (rom_push(list, &r) != 0)
			break;
	}
	closedir(d);
	qsort(list->items, list->count, sizeof(rom_t), cmp_rom);
	return 0;
}

void rom_list_free(rom_list_t *list)
{
	free(list->items);
	list->items = NULL;
	list->count = list->cap = 0;
}

const char *chip_name(chip_t c)
{
	switch (c) {
	case CHIP_DSP: return "DSP-1";
	case CHIP_SUPERFX: return "Super FX";
	case CHIP_SA1: return "SA-1";
	case CHIP_SDD1: return "SDD-1";
	case CHIP_SRTC: return "S-RTC";
	case CHIP_C4: return "C4";
	case CHIP_SPC7110: return "SPC7110";
	case CHIP_OTHER: return "chip especial";
	default: return "";
	}
}

int chip_supported(chip_t c)
{
	return c == CHIP_NONE || c == CHIP_DSP;
}

const char *region_short(region_t r)
{
	switch (r) {
	case REGION_JP: case REGION_US: case REGION_OTHER_NTSC: return "NTSC";
	case REGION_EU: case REGION_OTHER_PAL: return "PAL";
	default: return "?";
	}
}

const char *region_long(region_t r)
{
	switch (r) {
	case REGION_JP: return "NTSC \xC2\xB7 Jap\xC3\xA3o";
	case REGION_US: return "NTSC \xC2\xB7 Am\xC3\xA9rica";
	case REGION_EU: return "PAL \xC2\xB7 Europa";
	case REGION_OTHER_NTSC: return "NTSC";
	case REGION_OTHER_PAL: return "PAL";
	default: return "Regi\xC3\xA3o desconhecida";
	}
}
