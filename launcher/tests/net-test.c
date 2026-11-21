/*
 * Teste do modo rede sem tela: usa a rede falsa (tests/fake-rede.sh) e o
 * snespi-upload de verdade, manda um arquivo com o curl e confere o estado
 * que as telas mostram.
 *
 *   net-test CAMINHO_DA_REDE_FALSA CAMINHO_DO_SNESPI_UPLOAD
 */
#define _GNU_SOURCE
#include "../src/net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

#define CHECK(cond, msg) do { \
		if (!(cond)) { \
			printf("FALHOU: %s\n", msg); \
			fails++; \
		} \
	} while (0)

/* Roda net_poll até a fase mudar para outra que não seja "from" (ou 15 s) */
static void wait_leave(net_state_t *n, net_phase_t from)
{
	for (int i = 0; i < 300 && n->phase == from; i++) {
		net_poll(n);
		usleep(50 * 1000);
	}
}

static void pump(net_state_t *n, int ms)
{
	for (int i = 0; i < ms / 50; i++) {
		net_poll(n);
		usleep(50 * 1000);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return 2;
	char dir[] = "/tmp/snespi-net-test-XXXXXX";
	if (!mkdtemp(dir))
		return 2;
	char cmd[1024], path[512];
	net_state_t n;
	memset(&n, 0, sizeof(n));
	net_config(dir, argv[1], argv[2], 18099);

	/* cabo: conecta, sobe o servidor e mostra endereço com PIN */
	net_start(&n);
	CHECK(n.phase == NET_CONNECTING && strlen(n.pin) == 4, "net_start entra em CONNECTING com PIN");
	wait_leave(&n, NET_CONNECTING);
	CHECK(n.phase == NET_READY, "chega em READY");
	CHECK(strstr(n.url, ":18099/") && strstr(n.url, n.pin) && n.ssid[0] == 0, "endereço com porta e PIN");

	/* um jogo e um arquivo recusado */
	snprintf(cmd, sizeof(cmd),
		 "printf 'rom de teste' > /tmp/snespi-net-jogo.sfc && printf x > /tmp/snespi-net-doc.pdf && "
		 "curl -s -o /dev/null -F 'arquivo=@/tmp/snespi-net-jogo.sfc;filename=Jogo Novo.sfc' "
		 "-F 'arquivo=@/tmp/snespi-net-doc.pdf' http://127.0.0.1:18099/%s/enviar", n.pin);
	CHECK(system(cmd) == 0, "curl enviou");
	pump(&n, 500);
	CHECK(n.ok_count == 1 && n.err_count == 1 && n.nfiles == 2, "contagem de recebidos e recusados");
	CHECK(n.nfiles == 2 && !n.files[0].ok && !strcmp(n.files[0].why, "tipo"), "o mais recente é o recusado");
	CHECK(n.nfiles == 2 && n.files[1].ok && !strcmp(n.files[1].name, "Jogo Novo.sfc") &&
	      n.files[1].bytes == 12, "jogo recebido com nome e tamanho");
	CHECK(n.receiving[0] == 0, "nada mais chegando");
	snprintf(path, sizeof(path), "%s/Jogo Novo.sfc", dir);
	CHECK(access(path, F_OK) == 0, "arquivo gravado na pasta");

	/* procurar redes: nomes repetidos viram um, \xNN vira UTF-8, oculta some */
	net_scan(&n);
	wait_leave(&n, NET_SCANNING);
	CHECK(n.phase == NET_WIFI_LIST && n.naps == 3, "três redes na lista");
	CHECK(n.naps == 3 && !strcmp(n.aps[0].ssid, "Rede de Casa") && n.aps[0].signal == -48 && n.aps[0].secure,
	      "a mais forte primeiro, sem repetir");
	CHECK(n.naps == 3 && !strcmp(n.aps[1].ssid, "Fam\xC3\xADlia Silva"), "nome com acento decodificado");
	CHECK(n.naps == 3 && !n.aps[2].secure, "rede aberta");
	snprintf(cmd, sizeof(cmd), "curl -s -m 2 -o /dev/null http://127.0.0.1:18099/%s", n.pin);
	CHECK(system(cmd) != 0, "servidor parado enquanto escolhe a rede");

	/* escolher a rede grava wifi.txt e conecta de novo, com outro PIN */
	setenv("SNESPI_FAKE_WIFI", "1", 1);
	net_wifi_connect(&n, n.aps[1].ssid, "senha com espa\xC3\xA7o \"e aspas\"");
	wait_leave(&n, NET_CONNECTING);
	CHECK(n.phase == NET_READY && !strcmp(n.ssid, "Fam\xC3\xADlia Silva") && n.has_wifi, "conectado pelo Wi-Fi");
	snprintf(path, sizeof(path), "%s/wifi.txt", dir);
	FILE *fp = fopen(path, "r");
	char l1[128] = "", l2[128] = "";
	if (fp) {
		if (!fgets(l1, sizeof(l1), fp) || !fgets(l2, sizeof(l2), fp))
			l1[0] = 0;
		fclose(fp);
	}
	CHECK(!strcmp(l1, "Fam\xC3\xADlia Silva\n") && !strcmp(l2, "senha com espa\xC3\xA7o \"e aspas\"\n"),
	      "wifi.txt com rede e senha");

	/* erro do script vira tela de erro, com o motivo */
	setenv("SNESPI_FAKE_REDE", "wifi-falhou", 1);
	net_start(&n);
	wait_leave(&n, NET_CONNECTING);
	CHECK(n.phase == NET_ERROR && !strcmp(n.err, "wifi-falhou") && n.has_wifi, "erro wifi-falhou");
	unsetenv("SNESPI_FAKE_REDE");

	/* script que não existe */
	net_config(dir, "/nao/existe", argv[2], 18099);
	net_start(&n);
	wait_leave(&n, NET_CONNECTING);
	CHECK(n.phase == NET_ERROR && !strcmp(n.err, "interno"), "script ausente dá erro interno");

	/* sair: servidor parado */
	net_config(dir, argv[1], argv[2], 18099);
	net_start(&n);
	wait_leave(&n, NET_CONNECTING);
	CHECK(n.phase == NET_READY, "READY de novo");
	net_stop(&n);
	CHECK(n.phase == NET_OFF, "net_stop deixa em OFF");
	snprintf(cmd, sizeof(cmd), "curl -s -m 2 -o /dev/null http://127.0.0.1:18099/%s", n.pin);
	CHECK(system(cmd) != 0, "servidor parado depois de sair");

	snprintf(cmd, sizeof(cmd), "rm -rf '%s' /tmp/snespi-net-jogo.sfc /tmp/snespi-net-doc.pdf", dir);
	if (system(cmd) != 0)
		fails++;
	printf(fails ? "net-test: %d falha(s)\n" : "net-test: ok\n", fails);
	return fails ? 1 : 0;
}
