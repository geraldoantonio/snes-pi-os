/*
 * Teste no Linux (roda dentro do QEMU, ver scripts/uhid-container.sh):
 * entrega ao kernel, pelo uhid, o descritor HID do firmware e confere o
 * controle que ele cria e os eventos de cada entrada. Os relatórios vêm de
 * pad_report(), o mesmo código que roda no Pico.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <linux/uhid.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "hid_report_desc.h"
#include "pad.h"

#define NAME "snes-pi-os Controle Arcade"

static int fails;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		printf("FALHOU: " __VA_ARGS__); \
		printf("\n"); \
		fails++; \
	} \
} while (0)

static int uhid_create(void)
{
	int fd = open("/dev/uhid", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("/dev/uhid");
		return -1;
	}
	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE2;
	strcpy((char *)ev.u.create2.name, NAME);
	memcpy(ev.u.create2.rd_data, desc_hid_report, sizeof(desc_hid_report));
	ev.u.create2.rd_size = sizeof(desc_hid_report);
	ev.u.create2.bus = BUS_USB;
	ev.u.create2.vendor = 0x1209;
	ev.u.create2.product = 0x0001;
	if (write(fd, &ev, sizeof(ev)) < 0) {
		perror("UHID_CREATE2");
		return -1;
	}
	return fd;
}

static void uhid_send(int fd, uint16_t state)
{
	struct uhid_event ev;
	pad_report_t r;
	pad_report(state, &r);
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT2;
	ev.u.input2.size = sizeof(r);
	memcpy(ev.u.input2.data, &r, sizeof(r));
	if (write(fd, &ev, sizeof(ev)) < 0)
		perror("UHID_INPUT2");
}

/* Abre o primeiro /dev/input/<prefixo>N cujo nome é o do controle */
static int open_input(const char *prefix, unsigned long name_ioctl)
{
	for (int tries = 0; tries < 50; tries++) {
		for (int i = 0; i < 16; i++) {
			char path[64], name[128] = "";
			snprintf(path, sizeof(path), "/dev/input/%s%d", prefix, i);
			int fd = open(path, O_RDONLY | O_NONBLOCK);
			if (fd < 0)
				continue;
			ioctl(fd, name_ioctl, name);
			if (strcmp(name, NAME) == 0) {
				printf("%s: \"%s\"\n", path, name);
				return fd;
			}
			close(fd);
		}
		usleep(100000);
	}
	return -1;
}

#define TESTBIT(bit, arr) ((arr)[(bit) / 8] >> ((bit) % 8) & 1)

/* Lê os eventos pendentes; devolve o último EV_KEY ou EV_ABS visto */
static int read_ev(int fd, int *type, int *code, int *value)
{
	struct pollfd p = { fd, POLLIN, 0 };
	struct input_event ev;
	int got = 0;
	while (poll(&p, 1, 200) > 0 && read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
		if (ev.type == EV_KEY || ev.type == EV_ABS) {
			*type = ev.type;
			*code = ev.code;
			*value = ev.value;
			got++;
		}
		if (ev.type == EV_SYN && got)
			break;
	}
	return got;
}

static int read_js(int fd, struct js_event *out)
{
	struct pollfd p = { fd, POLLIN, 0 };
	struct js_event e;
	int got = 0;
	while (poll(&p, 1, 200) > 0 && read(fd, &e, sizeof(e)) == sizeof(e)) {
		if (e.type & JS_EVENT_INIT)
			continue;
		*out = e;
		got++;
		break;
	}
	return got;
}

int main(void)
{
	static const struct { const char *name; int in, type, code, value, js; } want[] = {
		{ "Cima", PAD_UP, EV_ABS, ABS_Y, 0, 1 },
		{ "Baixo", PAD_DOWN, EV_ABS, ABS_Y, 255, 1 },
		{ "Esquerda", PAD_LEFT, EV_ABS, ABS_X, 0, 0 },
		{ "Direita", PAD_RIGHT, EV_ABS, ABS_X, 255, 0 },
		/* códigos que o launcher espera (input_evdev.c) e número no snes9x.cfg */
		{ "A", PAD_A, EV_KEY, BTN_THUMB, 1, 1 },
		{ "B", PAD_B, EV_KEY, BTN_THUMB2, 1, 2 },
		{ "X", PAD_X, EV_KEY, BTN_TRIGGER, 1, 0 },
		{ "Y", PAD_Y, EV_KEY, BTN_TOP, 1, 3 },
		{ "L", PAD_L, EV_KEY, BTN_TOP2, 1, 4 },
		{ "R", PAD_R, EV_KEY, BTN_PINKIE, 1, 5 },
		{ "Select", PAD_SELECT, EV_KEY, BTN_BASE3, 1, 8 },
		{ "Start", PAD_START, EV_KEY, BTN_BASE4, 1, 9 },
	};

	int uh = uhid_create();
	if (uh < 0)
		return 1;
	int evfd = open_input("event", EVIOCGNAME(128));
	int jsfd = open_input("js", JSIOCGNAME(128));
	CHECK(evfd >= 0, "o kernel não criou o /dev/input/eventN");
	CHECK(jsfd >= 0, "o kernel não criou o /dev/input/jsN");
	if (evfd < 0)
		return 1;

	unsigned char keys[KEY_MAX / 8 + 1] = { 0 }, absb[ABS_MAX / 8 + 1] = { 0 };
	ioctl(evfd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
	ioctl(evfd, EVIOCGBIT(EV_ABS, sizeof(absb)), absb);
	printf("botões:");
	int nkeys = 0;
	for (int c = 0; c <= KEY_MAX; c++)
		if (TESTBIT(c, keys)) {
			printf(" 0x%x", c);
			nkeys++;
		}
	printf("\neixos:");
	int nabs = 0;
	for (int c = 0; c <= ABS_MAX; c++)
		if (TESTBIT(c, absb)) {
			struct input_absinfo ai;
			ioctl(evfd, EVIOCGABS(c), &ai);
			printf(" %d (%d..%d)", c, ai.minimum, ai.maximum);
			nabs++;
		}
	printf("\n");
	CHECK(nkeys == 10 && TESTBIT(BTN_TRIGGER, keys) && TESTBIT(BTN_BASE4, keys),
	      "esperava 10 botões, de BTN_TRIGGER a BTN_BASE4");
	CHECK(!TESTBIT(BTN_SOUTH, keys), "não devia ter BTN_SOUTH (convenção do Xbox)");
	CHECK(nabs == 2 && TESTBIT(ABS_X, absb) && TESTBIT(ABS_Y, absb), "esperava só ABS_X e ABS_Y");

	if (jsfd >= 0) {
		char nb = 0, na = 0;
		ioctl(jsfd, JSIOCGBUTTONS, &nb);
		ioctl(jsfd, JSIOCGAXES, &na);
		printf("joydev: %d botões, %d eixos\n", nb, na);
		CHECK(nb == 10 && na == 2, "joydev devia ter 10 botões e 2 eixos");
	}

	/* estado inicial e limpeza do que ficou na fila */
	int t, c, v;
	struct js_event je;
	uhid_send(uh, 0);
	while (read_ev(evfd, &t, &c, &v))
		;
	while (jsfd >= 0 && read_js(jsfd, &je))
		;

	for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++) {
		uhid_send(uh, (uint16_t)(1u << want[i].in));
		t = c = v = -1;
		int got = read_ev(evfd, &t, &c, &v);
		printf("%-8s evdev tipo %d código 0x%x valor %d", want[i].name, t, c, v);
		CHECK(got && t == want[i].type && c == want[i].code && v == want[i].value,
		      "%s: esperava tipo %d código 0x%x valor %d", want[i].name,
		      want[i].type, want[i].code, want[i].value);
		if (jsfd >= 0) {
			memset(&je, 0, sizeof(je));
			got = read_js(jsfd, &je);
			int isbtn = want[i].type == EV_KEY;
			printf(" | joydev %s %d valor %d", (je.type & JS_EVENT_BUTTON) ? "botão" : "eixo",
			       je.number, je.value);
			CHECK(got && je.number == want[i].js &&
			      !!(je.type & JS_EVENT_BUTTON) == isbtn,
			      "%s: joydev esperava %s %d", want[i].name, isbtn ? "botão" : "eixo",
			      want[i].js);
			/* o PiSNES só aceita a direção além de +-10000 */
			if (!isbtn)
				CHECK(want[i].value ? je.value > 10000 : je.value < -10000,
				      "%s: eixo %d não passa do limiar do PiSNES", want[i].name, je.value);
		}
		printf("\n");
		uhid_send(uh, 0);	/* solta */
		t = c = v = -1;
		got = read_ev(evfd, &t, &c, &v);
		CHECK(got && c == want[i].code && v == (want[i].type == EV_ABS ? 127 : 0),
		      "%s: ao soltar veio código 0x%x valor %d", want[i].name, c, v);
		if (jsfd >= 0) {
			got = read_js(jsfd, &je);
			CHECK(got && je.value == 0, "%s: joydev não voltou a zero (%d)", want[i].name,
			      je.value);
		}
	}

	printf(fails ? "RESULTADO: %d FALHA(S)\n" : "RESULTADO: OK\n", fails);
	return fails != 0;
}
