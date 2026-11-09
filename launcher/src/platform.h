/* Interface de plataforma: vídeo e entrada. */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "gfx.h"

/* ---- vídeo ---- */

/* Abre a saída com a resolução lógica w x h; devolve a superfície de desenho */
surface_t *video_open(int w, int h);
void video_present(void);
/* Libera a tela para outro programa (o emulador) e a retoma depois */
void video_suspend(void);
int video_resume(void);
void video_close(void);
/*
 * Sai deixando a imagem na tela (tela de abertura do boot): não apaga o
 * framebuffer e mantém o terminal em modo gráfico, para o texto do console
 * não aparecer por cima até o launcher abrir.
 */
void video_detach(void);

/* ---- entrada ---- */

enum {
	IN_NONE = 0,
	IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT,
	IN_A, IN_B, IN_X, IN_Y, IN_L, IN_R,
	IN_START, IN_SELECT,
	IN_POWER,	/* botão de desligar (GPIO) ou tecla Power */
	IN_COUNT
};

typedef struct {
	int button;	/* IN_* */
	int pressed;	/* 1 apertou, 0 soltou */
} in_event_t;

int input_open(void);
void input_close(void);
/* Espera até timeout_ms por um evento; devolve 1 se preencheu ev */
int input_poll(in_event_t *ev, int timeout_ms);
/* Número de controles (não teclados) conectados */
int input_pad_count(void);

/*
 * Botão de desligar enquanto o emulador roda (a entrada normal fica
 * fechada nessa hora): power_watch_pressed devolve 1 se foi apertado
 * desde a última chamada. Não bloqueia.
 */
void power_watch_open(void);
int power_watch_pressed(void);
void power_watch_close(void);

/* ---- sistema ---- */

/*
 * Desliga (reboot = 0) ou reinicia o aparelho pelo init, que fecha os
 * programas e desmonta /roms. Devolve 0 se o desligamento começou, 1 se a
 * plataforma não desliga (desenvolvimento: o launcher só fecha) e -1 se falhou.
 */
int system_power(int reboot);

/* LED de status (/sys/class/leds/snespi-status); sem o LED, não faz nada */
enum { LED_ON, LED_BLINK, LED_OFF };
void system_led(int mode);

#endif
