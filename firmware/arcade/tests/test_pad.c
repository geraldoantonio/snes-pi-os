/* Testes da lógica do controle arcade (make test) */
#include <stdio.h>
#include <string.h>

#include "pad.h"
#include "pins.h"

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

#define BIT(i) ((uint16_t)(1u << (i)))

static void test_report(void)
{
	pad_report_t r;

	pad_report(0, &r);
	CHECK(r.x == 0x7f && r.y == 0x7f && r.buttons[0] == 0 && r.buttons[1] == 0);

	pad_report(BIT(PAD_LEFT) | BIT(PAD_UP), &r);
	CHECK(r.x == 0x00 && r.y == 0x00);
	pad_report(BIT(PAD_RIGHT) | BIT(PAD_DOWN), &r);
	CHECK(r.x == 0xff && r.y == 0xff);
	/* opostos juntos = centro */
	pad_report(BIT(PAD_LEFT) | BIT(PAD_RIGHT) | BIT(PAD_UP) | BIT(PAD_DOWN), &r);
	CHECK(r.x == 0x7f && r.y == 0x7f);

	/* numeração do snes9x.cfg do overlay: X=0 A=1 B=2 Y=3 L=4 R=5 Select=8 Start=9 */
	static const struct { int in, button; } map[] = {
		{ PAD_X, 0 }, { PAD_A, 1 }, { PAD_B, 2 }, { PAD_Y, 3 },
		{ PAD_L, 4 }, { PAD_R, 5 }, { PAD_SELECT, 8 }, { PAD_START, 9 },
	};
	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		pad_report(BIT(map[i].in), &r);
		unsigned got = r.buttons[0] | (unsigned)r.buttons[1] << 8;
		CHECK(got == 1u << map[i].button);
		CHECK(r.x == 0x7f && r.y == 0x7f);
	}
	CHECK(sizeof(pad_report_t) == 4);
}

static void test_debounce(void)
{
	pad_t p;
	pad_init(&p);

	/* aperto vale na primeira leitura */
	CHECK(pad_update(&p, BIT(PAD_A)) == BIT(PAD_A));
	/* repique logo depois é ignorado */
	for (int i = 0; i < PAD_DEBOUNCE_MS; i++)
		CHECK(pad_update(&p, i % 2 ? BIT(PAD_A) : 0) == BIT(PAD_A));
	/* passado o tempo, soltar vale na hora */
	CHECK(pad_update(&p, 0) == 0);
	/* e o repique ao soltar também é ignorado */
	CHECK(pad_update(&p, BIT(PAD_A)) == 0);

	/* uma entrada travada não atrasa as outras */
	pad_init(&p);
	pad_update(&p, BIT(PAD_A));
	CHECK(pad_update(&p, BIT(PAD_A) | BIT(PAD_START)) == (BIT(PAD_A) | BIT(PAD_START)));

	/* aperto mais curto que a trava: solta sozinho quando ela acaba */
	pad_init(&p);
	pad_update(&p, BIT(PAD_B));
	uint16_t s = BIT(PAD_B);
	for (int i = 0; i <= PAD_DEBOUNCE_MS; i++)
		s = pad_update(&p, 0);
	CHECK(s == 0);
}

static void test_pins(void)
{
	/* nenhum pino repetido, nenhum fora dos GPIOs expostos no Pico */
	unsigned long seen = 0;
	for (int p = 0; p < 2; p++) {
		for (int i = 0; i < PAD_INPUTS; i++) {
			int pin = arcade_pins[p][i];
			CHECK(pin <= 22 || (pin >= 26 && pin <= 28));
			CHECK(!(seen >> pin & 1));
			seen |= 1ul << pin;
		}
	}
}

int main(void)
{
	test_report();
	test_debounce();
	test_pins();
	if (fails) {
		printf("%d falha(s)\n", fails);
		return 1;
	}
	printf("controle arcade: testes OK\n");
	return 0;
}
