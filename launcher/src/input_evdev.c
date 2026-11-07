/*
 * Entrada pelo evdev (/dev/input/event*): teclado e controles USB.
 *
 * Controles com a convenção do kernel (Xbox, PlayStation, 8BitDo em modo
 * XInput): botão de baixo (A do Xbox) confirma, o da direita (B) volta e o
 * da esquerda (X) favorita, para os rótulos da tela baterem com o Xbox.
 * Controles genéricos estilo SNES (só BTN_TRIGGER..) seguem o layout do
 * SNES: botão da direita (A) confirma. O PiSNES tem mapeamento próprio.
 */
#include "platform.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_DEV 8
#define QUEUE 32
#define BITS(n) (((n) + 8 * sizeof(long) - 1) / (8 * sizeof(long)))
#define TEST(bit, arr) (((arr)[(bit) / (8 * sizeof(long))] >> ((bit) % (8 * sizeof(long)))) & 1)

typedef struct {
	int fd;
	int is_pad;
	char path[32];
	struct { int min, max; } abs[ABS_HAT0Y + 1];
	int dir[2][2];	/* [eixo X/Y][-1 / +1] pressionado */
} dev_t_;

static dev_t_ devs[MAX_DEV];
static int ndev;
static in_event_t queue[QUEUE];
static int q_head, q_tail;
static long long last_scan_ms;

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void push(int button, int pressed)
{
	int next = (q_tail + 1) % QUEUE;
	if (next == q_head || button == IN_NONE)
		return;
	queue[q_tail].button = button;
	queue[q_tail].pressed = pressed;
	q_tail = next;
}

static int map_key(int code)
{
	switch (code) {
	/* teclado (inclui as teclas padrão do PiSNES: d c s x a f) */
	case KEY_UP: return IN_UP;
	case KEY_DOWN: return IN_DOWN;
	case KEY_LEFT: return IN_LEFT;
	case KEY_RIGHT: return IN_RIGHT;
	case KEY_ENTER: case KEY_KPENTER: case KEY_D: return IN_A;
	case KEY_ESC: case KEY_BACKSPACE: case KEY_C: return IN_B;
	case KEY_S: return IN_X;
	case KEY_X: return IN_Y;
	case KEY_PAGEUP: case KEY_A: return IN_L;
	case KEY_PAGEDOWN: case KEY_F: return IN_R;
	case KEY_SPACE: return IN_START;
	case KEY_TAB: return IN_SELECT;
	/* controles pela convenção do kernel */
	case BTN_SOUTH: return IN_A;
	case BTN_EAST: return IN_B;
	case BTN_WEST: return IN_X;
	case BTN_NORTH: return IN_Y;
	case BTN_TL: return IN_L;
	case BTN_TR: return IN_R;
	case BTN_START: return IN_START;
	case BTN_SELECT: return IN_SELECT;
	case BTN_DPAD_UP: return IN_UP;
	case BTN_DPAD_DOWN: return IN_DOWN;
	case BTN_DPAD_LEFT: return IN_LEFT;
	case BTN_DPAD_RIGHT: return IN_RIGHT;
	/* controles genéricos estilo SNES (BTN_JOYSTICK...) (verificar) */
	case BTN_TRIGGER: return IN_X;
	case BTN_THUMB: return IN_A;
	case BTN_THUMB2: return IN_B;
	case BTN_TOP: return IN_Y;
	case BTN_TOP2: return IN_L;
	case BTN_PINKIE: return IN_R;
	case BTN_BASE3: return IN_SELECT;
	case BTN_BASE4: return IN_START;
	default: return IN_NONE;
	}
}

static void close_dev(int i)
{
	close(devs[i].fd);
	devs[i] = devs[--ndev];
}

static int known(const char *path)
{
	for (int i = 0; i < ndev; i++)
		if (strcmp(devs[i].path, path) == 0)
			return 1;
	return 0;
}

static void scan(void)
{
	DIR *d = opendir("/dev/input");
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) != NULL && ndev < MAX_DEV) {
		if (strncmp(e->d_name, "event", 5) != 0)
			continue;
		char path[32];
		snprintf(path, sizeof(path), "/dev/input/%.16s", e->d_name);
		if (known(path))
			continue;
		int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		unsigned long evbits[BITS(EV_MAX + 1)] = { 0 };
		unsigned long keybits[BITS(KEY_MAX + 1)] = { 0 };
		ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits);
		ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
		int is_pad = TEST(BTN_GAMEPAD, keybits) || TEST(BTN_JOYSTICK, keybits) ||
			     TEST(BTN_THUMB, keybits) || TEST(BTN_SOUTH, keybits);
		int is_kbd = TEST(KEY_ENTER, keybits) && TEST(KEY_UP, keybits);
		if (!TEST(EV_KEY, evbits) || (!is_pad && !is_kbd)) {
			close(fd);
			continue;
		}
		dev_t_ *dv = &devs[ndev++];
		memset(dv, 0, sizeof(*dv));
		dv->fd = fd;
		dv->is_pad = is_pad;
		snprintf(dv->path, sizeof(dv->path), "%s", path);
		int axes[] = { ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y };
		for (int k = 0; k < 4; k++) {
			struct input_absinfo ai;
			if (ioctl(fd, EVIOCGABS(axes[k]), &ai) == 0) {
				dv->abs[axes[k]].min = ai.minimum;
				dv->abs[axes[k]].max = ai.maximum;
			}
		}
	}
	closedir(d);
	last_scan_ms = now_ms();
}

int input_open(void)
{
	q_head = q_tail = 0;
	scan();
	return 0;
}

void input_close(void)
{
	while (ndev > 0)
		close_dev(ndev - 1);
}

int input_pad_count(void)
{
	int n = 0;
	for (int i = 0; i < ndev; i++)
		n += devs[i].is_pad;
	return n;
}

/* Converte um eixo em direcional digital, com zona morta de 1/4 */
static void axis(dev_t_ *dv, int code, int value)
{
	int which = (code == ABS_X || code == ABS_HAT0X) ? 0 : 1;
	int min = dv->abs[code].min, max = dv->abs[code].max;
	if (max <= min)
		return;
	int range = max - min;
	int pos = value - min;
	int neg = pos < range / 4, plus = pos > range - range / 4;
	int btn_neg = which ? IN_UP : IN_LEFT, btn_pos = which ? IN_DOWN : IN_RIGHT;
	if (neg != dv->dir[which][0]) {
		dv->dir[which][0] = neg;
		push(btn_neg, neg);
	}
	if (plus != dv->dir[which][1]) {
		dv->dir[which][1] = plus;
		push(btn_pos, plus);
	}
}

static void drain(int i)
{
	struct input_event ev[16];
	for (;;) {
		ssize_t n = read(devs[i].fd, ev, sizeof(ev));
		if (n < 0) {
			if (errno == ENODEV)	/* desconectado */
				close_dev(i);
			return;
		}
		if (n == 0)
			return;
		for (size_t k = 0; k < (size_t)n / sizeof(ev[0]); k++) {
			if (ev[k].type == EV_KEY && ev[k].value != 2)	/* ignora autorepeat */
				push(map_key(ev[k].code), ev[k].value);
			else if (ev[k].type == EV_ABS && ev[k].code <= ABS_HAT0Y &&
				 (ev[k].code == ABS_X || ev[k].code == ABS_Y ||
				  ev[k].code == ABS_HAT0X || ev[k].code == ABS_HAT0Y))
				axis(&devs[i], ev[k].code, ev[k].value);
		}
	}
}

int input_poll(in_event_t *out, int timeout_ms)
{
	if (q_head == q_tail) {
		if (now_ms() - last_scan_ms > 2000)
			scan();	/* pega controles conectados depois */
		struct pollfd pfd[MAX_DEV];
		for (int i = 0; i < ndev; i++) {
			pfd[i].fd = devs[i].fd;
			pfd[i].events = POLLIN;
			pfd[i].revents = 0;
		}
		if (ndev == 0) {
			if (timeout_ms > 0)
				usleep((useconds_t)timeout_ms * 1000);
			return 0;
		}
		if (poll(pfd, (nfds_t)ndev, timeout_ms) <= 0)
			return 0;
		for (int i = ndev - 1; i >= 0; i--)
			if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP))
				drain(i);
	}
	if (q_head == q_tail)
		return 0;
	*out = queue[q_head];
	q_head = (q_head + 1) % QUEUE;
	return 1;
}
