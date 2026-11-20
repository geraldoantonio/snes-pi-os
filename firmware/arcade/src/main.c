/*
 * Controle arcade do snes-pi-os para o Raspberry Pi Pico (RP2040).
 *
 * Lê alavanca e botões nos GPIOs (ver pins.h) e aparece no USB como um
 * joystick HID por jogador, com o mesmo layout dos controles genéricos
 * estilo SNES. As entradas são lidas a cada 1 ms.
 *
 * Para regravar sem abrir o painel: ligue o cabo USB segurando Select e
 * Start do jogador 1; o Pico entra no modo BOOTSEL (unidade RPI-RP2).
 */
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "pad.h"
#include "pins.h"

static uint16_t read_raw(int player, uint32_t gpios)
{
	uint16_t raw = 0;
	for (int i = 0; i < PAD_INPUTS; i++)
		if (!(gpios >> arcade_pins[player][i] & 1))	/* ativo em nível baixo */
			raw |= (uint16_t)(1u << i);
	return raw;
}

int main(void)
{
	pad_t pads[ARCADE_PLAYERS];
	int dirty[ARCADE_PLAYERS];

	for (int p = 0; p < ARCADE_PLAYERS; p++) {
		pad_init(&pads[p]);
		dirty[p] = 1;	/* manda o estado inicial */
		for (int i = 0; i < PAD_INPUTS; i++) {
			gpio_init(arcade_pins[p][i]);
			gpio_set_dir(arcade_pins[p][i], GPIO_IN);
			gpio_pull_up(arcade_pins[p][i]);
		}
	}
#ifdef PICO_DEFAULT_LED_PIN
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

	sleep_ms(10);	/* os pull-ups estabilizam */
	uint16_t boot = read_raw(0, gpio_get_all());
	if ((boot >> PAD_SELECT & 1) && (boot >> PAD_START & 1))
		reset_usb_boot(0, 0);

	tud_init(0);

	uint64_t next = time_us_64();
	for (;;) {
		tud_task();

		if (time_us_64() >= next) {
			next += 1000;
			uint32_t gpios = gpio_get_all();
			for (int p = 0; p < ARCADE_PLAYERS; p++) {
				uint16_t before = pads[p].state;
				if (pad_update(&pads[p], read_raw(p, gpios)) != before)
					dirty[p] = 1;
			}
#ifdef PICO_DEFAULT_LED_PIN
			gpio_put(PICO_DEFAULT_LED_PIN, tud_mounted());	/* aceso = reconhecido */
#endif
		}

		for (int p = 0; p < ARCADE_PLAYERS; p++) {
			if (dirty[p] && tud_hid_n_ready((uint8_t)p)) {
				pad_report_t r;
				pad_report(pads[p].state, &r);
				if (tud_hid_n_report((uint8_t)p, 0, &r, sizeof(r)))
					dirty[p] = 0;
			}
		}
	}
}
