/*
 * Modo rede ("Receber jogos"): sobe a rede pelo script snespi-rede (cabo ou
 * Wi-Fi USB), inicia o servidor snespi-upload e acompanha os dois sem
 * travar a tela. Nada de rede fica ligado fora deste modo.
 */
#ifndef NET_H
#define NET_H

typedef enum {
	NET_OFF,
	NET_CONNECTING,	/* script "up" rodando */
	NET_READY,	/* servidor no ar: endereço e PIN na tela */
	NET_ERROR,	/* err diz o motivo */
	NET_SCANNING,	/* script "scan" rodando */
	NET_WIFI_LIST,	/* escolher a rede Wi-Fi */
	NET_WIFI_KEY,	/* digitar a senha */
} net_phase_t;

#define NET_MAX_FILES 5		/* últimos arquivos mostrados na tela */
#define NET_MAX_APS 8
#define NET_SSID_LEN 33		/* 32 bytes + NUL */
#define NET_KEY_MAX 63		/* limite do WPA */

typedef struct {
	char name[208];
	int ok;
	int cover;		/* 1: capa; 0: jogo */
	long long bytes;
	char why[16];		/* motivo do erro (tipo, grande, disco...) */
} net_file_t;

typedef struct {
	char ssid[NET_SSID_LEN];
	int signal;		/* dBm (negativo); mais perto de 0 é melhor */
	int secure;		/* pede senha */
} net_ap_t;

/* teclado da senha: KB_ROWS x KB_COLS, 3 camadas (minúsculas, maiúsculas, símbolos) */
#define KB_ROWS 4
#define KB_COLS 10
#define KB_LAYERS 3

typedef struct {
	net_phase_t phase;
	int tick;			/* avança enquanto conecta ou procura (animação) */

	/* NET_READY */
	char ip[48], pin[8], url[96];
	char ssid[NET_SSID_LEN];	/* rede Wi-Fi em uso; vazio = cabo */
	int port;
	int has_wifi;			/* há adaptador Wi-Fi no aparelho */
	int ok_count, err_count;
	net_file_t files[NET_MAX_FILES];	/* do mais recente para o mais antigo */
	int nfiles;
	char receiving[208];		/* arquivo chegando agora, ou vazio */

	/* NET_ERROR: sem-rede, sem-wifi, wifi-falhou, dhcp, servidor, bloqueado, tempo, interno */
	char err[24];

	/* NET_WIFI_LIST / NET_WIFI_KEY */
	net_ap_t aps[NET_MAX_APS];
	int naps, ap_sel;
	char key[NET_KEY_MAX + 1];
	int kb_row, kb_col, kb_layer;
} net_state_t;

/* Programas e pasta; chamar uma vez. port 80 no Pi. */
void net_config(const char *roms_dir, const char *net_cmd, const char *upload_cmd, int port);

/* Começa a conectar (NET_CONNECTING) */
void net_start(net_state_t *n);
/* Procura as redes Wi-Fi (NET_SCANNING, depois NET_WIFI_LIST) */
void net_scan(net_state_t *n);
/* Grava a rede escolhida e a senha em <roms>/wifi.txt e conecta de novo */
void net_wifi_connect(net_state_t *n, const char *ssid, const char *key);
/* Acompanha os processos; devolve 1 se a tela mudou */
int net_poll(net_state_t *n);
/* Para o servidor e derruba a rede (espera terminar); deixa em NET_OFF */
void net_stop(net_state_t *n);

/* Caractere da tecla (ASCII) na camada do teclado da senha */
char net_kb_char(int layer, int row, int col);

#endif
