/*
 * Descritores USB: um dispositivo com uma interface HID por jogador.
 *
 * O uso da coleção é Joystick (e não Gamepad) de propósito: assim o Linux
 * numera os botões a partir de BTN_TRIGGER, como nos controles genéricos
 * estilo SNES, e não pela convenção do Xbox (BTN_SOUTH...).
 */
#include "pico/unique_id.h"
#include "tusb.h"

#include "hid_report_desc.h"
#include "pad.h"

/* VID 0x1209 (pid.codes), PID 0x0001: PID de teste, só para uso próprio */
#define USB_VID 0x1209
#define USB_PID 0x0001

enum { STR_LANG, STR_MAKER, STR_PRODUCT, STR_SERIAL, STR_PLAYER1, STR_PLAYER2 };

static const tusb_desc_device_t desc_device = {
	.bLength = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB = 0x0200,
	.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
	.idVendor = USB_VID,
	.idProduct = USB_PID,
	.bcdDevice = 0x0100,
	.iManufacturer = STR_MAKER,
	.iProduct = STR_PRODUCT,
	.iSerialNumber = STR_SERIAL,
	.bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void)
{
	return (const uint8_t *)&desc_device;
}

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance)
{
	(void)instance;
	return desc_hid_report;
}

#define CONFIG_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] = {
	TUD_CONFIG_DESCRIPTOR(1, CFG_TUD_HID, 0, CONFIG_LEN, 0, 100),
	/* interface, texto, protocolo, tamanho do descritor, endpoint, pacote, intervalo (ms) */
	TUD_HID_DESCRIPTOR(0, STR_PLAYER1, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
			   0x81, CFG_TUD_HID_EP_BUFSIZE, 1),
#if CFG_TUD_HID > 1
	TUD_HID_DESCRIPTOR(1, STR_PLAYER2, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
			   0x82, CFG_TUD_HID_EP_BUFSIZE, 1),
#endif
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
	(void)index;
	return desc_configuration;
}

/* Textos em ASCII (viram UTF-16 abaixo) */
static const char *const strings[] = {
	[STR_MAKER] = "snes-pi-os",
	[STR_PRODUCT] = "Controle Arcade",
	[STR_PLAYER1] = "Jogador 1",
	[STR_PLAYER2] = "Jogador 2",
};

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	static uint16_t desc[1 + 32];
	char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
	const char *s;
	size_t n = 0;

	(void)langid;
	if (index == STR_LANG) {
		desc[1] = 0x0409;	/* inglês (EUA) */
		n = 1;
	} else {
		if (index == STR_SERIAL) {
			pico_get_unique_board_id_string(serial, sizeof(serial));
			s = serial;
		} else if (index < sizeof(strings) / sizeof(strings[0]) && strings[index]) {
			s = strings[index];
		} else {
			return NULL;
		}
		for (; s[n] && n < 32; n++)
			desc[1 + n] = (uint8_t)s[n];
	}
	desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
	return desc;
}

/* O relatório só sai pelo endpoint de interrupção; o resto não é usado */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
			       uint8_t *buffer, uint16_t reqlen)
{
	(void)instance; (void)report_id; (void)type; (void)buffer; (void)reqlen;
	return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
			   const uint8_t *buffer, uint16_t bufsize)
{
	(void)instance; (void)report_id; (void)type; (void)buffer; (void)bufsize;
}
