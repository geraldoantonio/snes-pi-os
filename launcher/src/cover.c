#include "cover.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../third_party/stb_image.h"

#define SLOTS 6
#define MAX_PIXELS (4096 * 4096)

static const char *const EXTS[] = { "jpg", "jpeg", "png", "JPG", "JPEG", "PNG", NULL };

typedef struct {
	char stem[256];
	int box_w, box_h;
	surface_t *img;		/* NULL = sem capa (resultado negativo também fica no cache) */
	unsigned long used;
	int valid;
} slot_t;

static char dir[256];
static slot_t slots[SLOTS];
static unsigned long tick;

void cover_init(const char *covers_dir)
{
	snprintf(dir, sizeof(dir), "%s", covers_dir);
}

void cover_shutdown(void)
{
	for (int i = 0; i < SLOTS; i++) {
		gfx_surface_free(slots[i].img);
		memset(&slots[i], 0, sizeof(slots[i]));
	}
}

static int find_file(const char *stem, char *out, size_t outsz)
{
	struct stat st;
	for (int i = 0; EXTS[i]; i++) {
		snprintf(out, outsz, "%s/%s.%s", dir, stem, EXTS[i]);
		if (stat(out, &st) == 0 && S_ISREG(st.st_mode))
			return 1;
	}
	return 0;
}

int cover_has_file(const char *stem)
{
	char path[600];
	return find_file(stem, path, sizeof(path));
}

static slot_t *lookup(const char *stem, int bw, int bh)
{
	for (int i = 0; i < SLOTS; i++)
		if (slots[i].valid && slots[i].box_w == bw && slots[i].box_h == bh &&
		    strcmp(slots[i].stem, stem) == 0) {
			slots[i].used = ++tick;
			return &slots[i];
		}
	return NULL;
}

const surface_t *cover_peek(const char *stem, int bw, int bh)
{
	slot_t *s = lookup(stem, bw, bh);
	return s ? s->img : NULL;
}

/* Reduz por média de área (ou amplia por vizinho mais próximo) para caber na caixa */
static surface_t *fit(const unsigned char *pix, int w, int h, int bw, int bh)
{
	double k = (double)bw / w;
	if ((double)bh / h < k)
		k = (double)bh / h;
	int ow = (int)(w * k + 0.5), oh = (int)(h * k + 0.5);
	if (ow < 1) ow = 1;
	if (oh < 1) oh = 1;
	surface_t *s = gfx_surface_new(ow, oh);
	if (!s)
		return NULL;
	for (int y = 0; y < oh; y++) {
		int y0 = (int)((long)y * h / oh), y1 = (int)((long)(y + 1) * h / oh);
		if (y1 <= y0) y1 = y0 + 1;
		for (int x = 0; x < ow; x++) {
			int x0 = (int)((long)x * w / ow), x1 = (int)((long)(x + 1) * w / ow);
			if (x1 <= x0) x1 = x0 + 1;
			unsigned r = 0, g = 0, b = 0, n = 0;
			for (int yy = y0; yy < y1; yy++) {
				const unsigned char *p = pix + ((size_t)yy * w + x0) * 3;
				for (int xx = x0; xx < x1; xx++, p += 3) {
					r += p[0]; g += p[1]; b += p[2]; n++;
				}
			}
			r /= n; g /= n; b /= n;
			s->px[(size_t)y * ow + x] = rgb((r << 16) | (g << 8) | b);
		}
	}
	return s;
}

const surface_t *cover_get(const char *stem, int bw, int bh)
{
	slot_t *hit = lookup(stem, bw, bh);
	if (hit)
		return hit->img;

	surface_t *img = NULL;
	char path[600];
	if (find_file(stem, path, sizeof(path))) {
		int w, h, n;
		if (stbi_info(path, &w, &h, &n) && (long)w * h <= MAX_PIXELS) {
			unsigned char *rgb = stbi_load(path, &w, &h, &n, 3);
			if (rgb) {
				img = fit(rgb, w, h, bw, bh);
				stbi_image_free(rgb);
			}
		}
	}

	/* substitui o slot menos usado */
	slot_t *victim = &slots[0];
	for (int i = 0; i < SLOTS; i++) {
		if (!slots[i].valid) {
			victim = &slots[i];
			break;
		}
		if (slots[i].used < victim->used)
			victim = &slots[i];
	}
	gfx_surface_free(victim->img);
	snprintf(victim->stem, sizeof(victim->stem), "%s", stem);
	victim->box_w = bw;
	victim->box_h = bh;
	victim->img = img;
	victim->used = ++tick;
	victim->valid = 1;
	return img;
}
