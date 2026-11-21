/*
 * Descritor de relatório HID do controle arcade, separado para o teste no
 * Linux (tests/test_uhid.c) usar exatamente os mesmos bytes do firmware.
 */
#ifndef HID_REPORT_DESC_H
#define HID_REPORT_DESC_H

#include <stdint.h>

/* Relatório de 4 bytes (pad_report_t): X, Y e dez botões */
static const uint8_t desc_hid_report[] = {
	0x05, 0x01,		/* Usage Page (Generic Desktop) */
	0x09, 0x04,		/* Usage (Joystick) */
	0xa1, 0x01,		/* Collection (Application) */
	0x09, 0x01,		/*   Usage (Pointer) */
	0xa1, 0x00,		/*   Collection (Physical) */
	0x09, 0x30,		/*     Usage (X) */
	0x09, 0x31,		/*     Usage (Y) */
	0x15, 0x00,		/*     Logical Minimum (0) */
	0x26, 0xff, 0x00,	/*     Logical Maximum (255) */
	0x75, 0x08,		/*     Report Size (8) */
	0x95, 0x02,		/*     Report Count (2) */
	0x81, 0x02,		/*     Input (Data, Variable, Absolute) */
	0xc0,			/*   End Collection */
	0x05, 0x09,		/*   Usage Page (Button) */
	0x19, 0x01,		/*   Usage Minimum (1) */
	0x29, 0x0a,		/*   Usage Maximum (10) */
	0x15, 0x00,		/*   Logical Minimum (0) */
	0x25, 0x01,		/*   Logical Maximum (1) */
	0x75, 0x01,		/*   Report Size (1) */
	0x95, 0x0a,		/*   Report Count (10) */
	0x81, 0x02,		/*   Input (Data, Variable, Absolute) */
	0x95, 0x06,		/*   Report Count (6): completa o byte */
	0x81, 0x03,		/*   Input (Constant) */
	0xc0,			/* End Collection */
};

#endif
