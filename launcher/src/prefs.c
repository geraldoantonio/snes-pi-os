#include "prefs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FAV 512
#define NAME_LEN 256

static char dir[600];
static char favs[MAX_FAV][NAME_LEN];
static int nfav;
static char recent[PREFS_MAX_RECENT][NAME_LEN];
static int nrecent;

void prefs_init(const char *roms_dir)
{
	snprintf(dir, sizeof(dir), "%s/.snespi", roms_dir);
}

/* Lê até max linhas não vazias para out; devolve quantas */
static int read_lines(const char *name, char (*out)[NAME_LEN], int max)
{
	char path[700];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE *fp = fopen(path, "r");
	if (!fp)
		return 0;
	int n = 0;
	char line[NAME_LEN + 4];
	while (n < max && fgets(line, sizeof(line), fp)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!line[0])
			continue;
		snprintf(out[n], NAME_LEN, "%s", line);
		n++;
	}
	fclose(fp);
	return n;
}

/*
 * Grava num temporário, fsync e rename: se a energia cair no meio, fica o
 * arquivo antigo ou o novo, nunca um pela metade.
 */
static void write_lines(const char *name, char (*lines)[NAME_LEN], int n)
{
	if (mkdir(dir, 0755) != 0 && errno != EEXIST)
		return;
	char path[700], tmp[710];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *fp = fopen(tmp, "w");
	if (!fp)
		return;
	for (int i = 0; i < n; i++)
		fprintf(fp, "%s\n", lines[i]);
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
	rename(tmp, path);
}

void prefs_load(void)
{
	nfav = read_lines("favoritos.txt", favs, MAX_FAV);
	nrecent = read_lines("recentes.txt", recent, PREFS_MAX_RECENT);
}

int prefs_is_fav(const char *stem)
{
	for (int i = 0; i < nfav; i++)
		if (strcmp(favs[i], stem) == 0)
			return 1;
	return 0;
}

int prefs_toggle_fav(const char *stem)
{
	for (int i = 0; i < nfav; i++)
		if (strcmp(favs[i], stem) == 0) {
			memmove(favs[i], favs[i + 1], (size_t)(nfav - i - 1) * NAME_LEN);
			nfav--;
			write_lines("favoritos.txt", favs, nfav);
			return 0;
		}
	if (nfav >= MAX_FAV)
		return 0;
	snprintf(favs[nfav++], NAME_LEN, "%s", stem);
	write_lines("favoritos.txt", favs, nfav);
	return 1;
}

void prefs_add_recent(const char *stem)
{
	int at = nrecent;
	for (int i = 0; i < nrecent; i++)
		if (strcmp(recent[i], stem) == 0) {
			at = i;
			break;
		}
	if (at == nrecent && nrecent < PREFS_MAX_RECENT)
		nrecent++;
	if (at >= nrecent)
		at = nrecent - 1;	/* lista cheia: o mais antigo sai */
	memmove(recent[1], recent[0], (size_t)at * NAME_LEN);
	snprintf(recent[0], NAME_LEN, "%s", stem);
	write_lines("recentes.txt", recent, nrecent);
}

int prefs_recent_count(void)
{
	return nrecent;
}

const char *prefs_recent(int i)
{
	return (i >= 0 && i < nrecent) ? recent[i] : NULL;
}
