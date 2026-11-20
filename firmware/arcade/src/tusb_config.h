#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifndef ARCADE_PLAYERS
#define ARCADE_PLAYERS 2
#endif

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE 64

/* Uma interface HID por jogador: o Linux cria um joystick para cada uma */
#define CFG_TUD_HID ARCADE_PLAYERS
#define CFG_TUD_HID_EP_BUFSIZE 16

#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#endif
