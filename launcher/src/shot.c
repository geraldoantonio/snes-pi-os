/* Grava a superfície RGB565 como PNG (capturas para conferir o layout). */
#include "gfx.h"

#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "../third_party/stb_image_write.h"

int shot_write_png(const surface_t *s, const char *path)
{
	unsigned char *rgb = malloc((size_t)s->w * s->h * 3);
	if (!rgb)
		return -1;
	for (int y = 0; y < s->h; y++)
		for (int x = 0; x < s->w; x++) {
			uint16_t c = s->px[(size_t)y * s->stride + x];
			unsigned char *p = rgb + ((size_t)y * s->w + x) * 3;
			unsigned r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
			p[0] = (unsigned char)(r << 3 | r >> 2);
			p[1] = (unsigned char)(g << 2 | g >> 4);
			p[2] = (unsigned char)(b << 3 | b >> 2);
		}
	int ok = stbi_write_png(path, s->w, s->h, 3, rgb, s->w * 3);
	free(rgb);
	return ok ? 0 : -1;
}
