/*
 * Vídeo pelo framebuffer do Linux (/dev/fb0).
 *
 * No Pi com a pilha legada, o framebuffer pode ter qualquer tamanho: a GPU
 * escala para a saída HDMI. Pedimos 1280x720 a 16 bpp; se o driver recusar,
 * a imagem é centralizada no tamanho que ele aceitar.
 */
#include "platform.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

static int fb_fd = -1, tty_fd = -1;
static uint8_t *fb_mem;
static size_t fb_len;
static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static struct termios tio_saved;
static int tio_ok;
static surface_t *back;
static int want_w, want_h;

static void tty_graphics(int on)
{
	if (tty_fd < 0)
		return;
	ioctl(tty_fd, KDSETMODE, on ? KD_GRAPHICS : KD_TEXT);
	if (tio_ok) {
		if (on) {
			struct termios t = tio_saved;
			t.c_lflag &= ~(ICANON | ECHO);
			tcsetattr(tty_fd, TCSANOW, &t);
		} else {
			tcsetattr(tty_fd, TCSANOW, &tio_saved);
		}
	}
	tcflush(tty_fd, TCIFLUSH);
}

static int fb_setup(void)
{
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) == 0) {
		struct fb_var_screeninfo v = var;
		v.xres = v.xres_virtual = (unsigned)want_w;
		v.yres = v.yres_virtual = (unsigned)want_h;
		v.xoffset = v.yoffset = 0;
		v.bits_per_pixel = 16;
		ioctl(fb_fd, FBIOPUT_VSCREENINFO, &v);	/* pode falhar: usamos o que vier */
	}
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) != 0 ||
	    ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) != 0) {
		perror("snespi-launcher: framebuffer");
		return -1;
	}
	if (var.bits_per_pixel != 16 && var.bits_per_pixel != 32) {
		fprintf(stderr, "snespi-launcher: %u bpp não suportado\n", var.bits_per_pixel);
		return -1;
	}
	if (fb_mem)
		munmap(fb_mem, fb_len);
	fb_len = fix.smem_len;
	fb_mem = mmap(NULL, fb_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (fb_mem == MAP_FAILED) {
		fb_mem = NULL;
		perror("snespi-launcher: mmap");
		return -1;
	}
	memset(fb_mem, 0, fb_len);
	return 0;
}

surface_t *video_open(int w, int h)
{
	want_w = w;
	want_h = h;

	tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (tty_fd < 0)
		tty_fd = open("/dev/tty1", O_RDWR | O_CLOEXEC);
	if (tty_fd >= 0)
		tio_ok = tcgetattr(tty_fd, &tio_saved) == 0;

	fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (fb_fd < 0) {
		perror("snespi-launcher: /dev/fb0");
		return NULL;
	}
	if (fb_setup() != 0)
		return NULL;
	back = gfx_surface_new(w, h);
	if (!back)
		return NULL;
	tty_graphics(1);
	return back;
}

void video_present(void)
{
	if (!fb_mem || !back)
		return;
	int fw = (int)var.xres, fh = (int)var.yres;
	int ox = (fw - back->w) / 2, oy = (fh - back->h) / 2;
	int sx = ox < 0 ? -ox : 0, sy = oy < 0 ? -oy : 0;
	if (ox < 0) ox = 0;
	if (oy < 0) oy = 0;
	int w = back->w - sx, h = back->h - sy;
	if (w > fw) w = fw;
	if (h > fh) h = fh;

	for (int y = 0; y < h; y++) {
		const uint16_t *src = back->px + (size_t)(sy + y) * back->stride + sx;
		uint8_t *row = fb_mem + (size_t)(oy + y) * fix.line_length;
		if (var.bits_per_pixel == 16) {
			memcpy(row + (size_t)ox * 2, src, (size_t)w * 2);
		} else {
			uint32_t *dst = (uint32_t *)row + ox;
			for (int x = 0; x < w; x++) {
				uint16_t c = src[x];
				uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
				dst[x] = (r << 19 | (r >> 2) << 16) | (g << 10 | (g >> 4) << 8) |
					 (b << 3 | b >> 2);
			}
		}
	}
}

void video_suspend(void)
{
	if (fb_mem)
		memset(fb_mem, 0, fb_len);
	/* o emulador (SDL/fbcon) configura o terminal do jeito dele */
	if (tty_fd >= 0 && tio_ok)
		tcsetattr(tty_fd, TCSANOW, &tio_saved);
}

int video_resume(void)
{
	/* o SDL pode ter mudado o modo do framebuffer */
	if (fb_setup() != 0)
		return -1;
	tty_graphics(1);
	return 0;
}

void video_close(void)
{
	if (fb_mem) {
		memset(fb_mem, 0, fb_len);
		munmap(fb_mem, fb_len);
		fb_mem = NULL;
	}
	tty_graphics(0);
	if (fb_fd >= 0)
		close(fb_fd);
	if (tty_fd >= 0)
		close(tty_fd);
	fb_fd = tty_fd = -1;
	gfx_surface_free(back);
	back = NULL;
}
