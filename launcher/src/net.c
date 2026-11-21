#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TICK_MS 300
#define UP_TIMEOUT_MS 70000	/* cabo + Wi-Fi + DHCP, com folga */
#define SCAN_TIMEOUT_MS 30000
#define SERVER_START_MS 5000
#define DOWN_TIMEOUT_MS 8000

static char g_roms[600] = "/roms";
static char g_net_cmd[600] = "/usr/bin/snespi-rede";
static char g_upload_cmd[600] = "/usr/bin/snespi-upload";
static int g_port = 80;

/* processo filho com a saída padrão num pipe, lido sem bloquear */
typedef struct {
	pid_t pid;
	int fd;
	char buf[4096];
	size_t len;
	long long deadline;
} proc_t;

static proc_t job;	/* snespi-rede up / scan */
static proc_t server;	/* snespi-upload */
static long long next_tick;

static long long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void net_config(const char *roms_dir, const char *net_cmd, const char *upload_cmd, int port)
{
	snprintf(g_roms, sizeof(g_roms), "%s", roms_dir);
	if (net_cmd)
		snprintf(g_net_cmd, sizeof(g_net_cmd), "%s", net_cmd);
	if (upload_cmd)
		snprintf(g_upload_cmd, sizeof(g_upload_cmd), "%s", upload_cmd);
	if (port > 0)
		g_port = port;
}

static int spawn(proc_t *p, char *const argv[], int timeout_ms)
{
	int pfd[2];
	memset(p, 0, sizeof(*p));
	p->fd = -1;
	if (pipe(pfd) != 0)
		return -1;
	pid_t pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}
	if (pid == 0) {
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGPIPE, SIG_DFL);
		int nul = open("/dev/null", O_RDWR);
		if (nul >= 0) {
			dup2(nul, STDIN_FILENO);
			dup2(nul, STDERR_FILENO);
		}
		dup2(pfd[1], STDOUT_FILENO);
		/* tela, controles e som do launcher não passam para o filho */
		for (int fd = 3; fd < 256; fd++)
			close(fd);
		execv(argv[0], argv);
		_exit(127);
	}
	close(pfd[1]);
	fcntl(pfd[0], F_SETFL, fcntl(pfd[0], F_GETFL) | O_NONBLOCK);
	fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
	p->pid = pid;
	p->fd = pfd[0];
	p->deadline = now_ms() + timeout_ms;
	return 0;
}

/* Lê o que houver no pipe; o excesso sem quebra de linha é descartado */
static void proc_read(proc_t *p)
{
	if (p->fd < 0)
		return;
	for (;;) {
		if (p->len >= sizeof(p->buf) - 1)
			p->len = 0;
		ssize_t r = read(p->fd, p->buf + p->len, sizeof(p->buf) - 1 - p->len);
		if (r <= 0)
			break;
		p->len += (size_t)r;
	}
	p->buf[p->len] = 0;
}

/* Tira a próxima linha completa do buffer; 1 se havia */
static int proc_line(proc_t *p, char *out, size_t outsz)
{
	char *nl = memchr(p->buf, '\n', p->len);
	if (!nl)
		return 0;
	size_t n = (size_t)(nl - p->buf);
	snprintf(out, outsz, "%.*s", (int)n, p->buf);
	memmove(p->buf, nl + 1, p->len - n - 1);
	p->len -= n + 1;
	p->buf[p->len] = 0;
	return 1;
}

/* 1 se o processo terminou (status em *st) */
static int proc_exited(proc_t *p, int *st)
{
	if (p->pid <= 0)
		return 1;
	pid_t w = waitpid(p->pid, st, WNOHANG);
	if (w == 0)
		return 0;
	if (w < 0)
		*st = -1;
	p->pid = 0;
	return 1;
}

static void proc_kill(proc_t *p, int grace_ms)
{
	if (p->pid > 0) {
		int st;
		kill(p->pid, SIGTERM);
		long long until = now_ms() + grace_ms;
		while (waitpid(p->pid, &st, WNOHANG) == 0) {
			if (now_ms() >= until) {
				kill(p->pid, SIGKILL);
				waitpid(p->pid, &st, 0);
				break;
			}
			usleep(20 * 1000);
		}
		p->pid = 0;
	}
	if (p->fd >= 0) {
		close(p->fd);
		p->fd = -1;
	}
	p->len = 0;
}

/* Separa a linha nos TABs (altera a linha); devolve o número de campos */
static int split_tabs(char *line, char **f, int max)
{
	int n = 0;
	for (char *p = line; n < max; p++) {
		f[n++] = p;
		p = strchr(p, '\t');
		if (!p)
			break;
		*p = 0;
	}
	return n;
}

static void set_error(net_state_t *n, const char *code)
{
	proc_kill(&job, 500);
	proc_kill(&server, 2000);
	n->phase = NET_ERROR;
	snprintf(n->err, sizeof(n->err), "%s", code);
}

static void run_script(net_state_t *n, const char *verb, int timeout_ms)
{
	char *argv[] = { g_net_cmd, (char *)verb, g_roms, NULL };
	if (spawn(&job, argv, timeout_ms) != 0)
		set_error(n, "interno");
	next_tick = now_ms() + TICK_MS;
}

static void new_pin(net_state_t *n)
{
	unsigned v = 0;
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0 || read(fd, &v, sizeof(v)) != (ssize_t)sizeof(v))
		v = (unsigned)now_ms() * 2654435761u ^ (unsigned)getpid();
	if (fd >= 0)
		close(fd);
	snprintf(n->pin, sizeof(n->pin), "%04u", v % 10000);
}

void net_start(net_state_t *n)
{
	proc_kill(&job, 500);
	proc_kill(&server, 2000);
	n->phase = NET_CONNECTING;
	n->tick = 0;
	n->err[0] = n->ip[0] = n->url[0] = n->ssid[0] = n->receiving[0] = 0;
	n->nfiles = n->ok_count = n->err_count = 0;
	n->port = g_port;
	new_pin(n);
	run_script(n, "up", UP_TIMEOUT_MS);
}

void net_scan(net_state_t *n)
{
	proc_kill(&job, 500);
	proc_kill(&server, 2000);
	n->phase = NET_SCANNING;
	n->tick = 0;
	n->naps = n->ap_sel = 0;
	run_script(n, "scan", SCAN_TIMEOUT_MS);
}

/* <roms>/wifi.txt: 1ª linha o nome da rede, 2ª a senha (vazia = rede aberta) */
static int write_wifi(const char *ssid, const char *key)
{
	char path[700], tmp[710];
	snprintf(path, sizeof(path), "%s/wifi.txt", g_roms);
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *fp = fopen(tmp, "w");
	if (!fp)
		return -1;
	fprintf(fp, "%s\n%s\n", ssid, key);
	fflush(fp);
	fsync(fileno(fp));
	if (fclose(fp) != 0 || rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	int dfd = open(g_roms, O_RDONLY | O_DIRECTORY);
	if (dfd >= 0) {
		fsync(dfd);
		close(dfd);
	}
	return 0;
}

void net_wifi_connect(net_state_t *n, const char *ssid, const char *key)
{
	if (write_wifi(ssid, key) != 0) {
		set_error(n, "interno");
		return;
	}
	net_start(n);
}

/* ---- resultado do "up" ---- */

static void start_server(net_state_t *n)
{
	char port[12];
	snprintf(port, sizeof(port), "%d", n->port);
	char *argv[] = { g_upload_cmd, "--dir", g_roms, "--pin", n->pin, "--port", port, NULL };
	if (spawn(&server, argv, SERVER_START_MS) != 0)
		set_error(n, "servidor");
}

static void up_done(net_state_t *n, int st)
{
	/* vale a última linha: "ok <iface> <ip> <wifi> <rede>" ou "erro <motivo> <wifi>" */
	char line[512], last[512] = "";
	proc_read(&job);
	while (proc_line(&job, line, sizeof(line)))
		if (line[0])
			snprintf(last, sizeof(last), "%s", line);
	proc_kill(&job, 0);
	char *f[5];
	int nf = split_tabs(last, f, 5);
	if (nf >= 4 && !strcmp(f[0], "ok") && f[2][0]) {
		snprintf(n->ip, sizeof(n->ip), "%s", f[2]);
		n->has_wifi = atoi(f[3]);
		snprintf(n->ssid, sizeof(n->ssid), "%s", nf >= 5 ? f[4] : "");
		if (n->port == 80)
			snprintf(n->url, sizeof(n->url), "http://%s/%s", n->ip, n->pin);
		else
			snprintf(n->url, sizeof(n->url), "http://%s:%d/%s", n->ip, n->port, n->pin);
		start_server(n);	/* segue em NET_CONNECTING até o "pronto" */
		return;
	}
	if (nf >= 2 && !strcmp(f[0], "erro")) {
		n->has_wifi = nf >= 3 ? atoi(f[2]) : 0;
		set_error(n, f[1]);
		return;
	}
	(void)st;
	set_error(n, "interno");	/* script ausente ou saída inesperada */
}

/* ---- resultado do "scan" ---- */

/* Desfaz o formato do wpa_cli (\xNN, \\, \", \n...); 0 se o nome serve */
static int decode_ssid(const char *in, char *out, size_t outsz)
{
	size_t n = 0;
	for (const char *p = in; *p; p++) {
		unsigned c = (unsigned char)*p;
		if (c == '\\' && p[1]) {
			p++;
			switch (*p) {
			case 'x': {
				char hex[3] = { 0 };
				if (p[1] && p[2]) {
					hex[0] = p[1];
					hex[1] = p[2];
					p += 2;
				}
				c = (unsigned)strtoul(hex, NULL, 16);
				break;
			}
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'e': c = 27; break;
			default: c = (unsigned char)*p; break;
			}
		}
		if (c < 0x20 || c == 0x7f)
			return -1;	/* não dá para mostrar nem guardar no wifi.txt */
		if (n + 1 >= outsz)
			return -1;
		out[n++] = (char)c;
	}
	out[n] = 0;
	return n > 0 ? 0 : -1;	/* rede oculta: sem nome */
}

static int ap_cmp(const void *a, const void *b)
{
	return ((const net_ap_t *)b)->signal - ((const net_ap_t *)a)->signal;
}

static void scan_done(net_state_t *n)
{
	static net_ap_t all[48];
	int count = 0;
	char line[512];
	proc_read(&job);
	n->naps = n->ap_sel = 0;
	while (proc_line(&job, line, sizeof(line))) {
		char *f[5];
		int nf = split_tabs(line, f, 5);
		if (nf >= 2 && !strcmp(f[0], "erro")) {
			n->has_wifi = nf >= 3 ? atoi(f[2]) : 0;
			set_error(n, f[1]);
			return;
		}
		/* linha do wpa_cli scan_results: bssid, frequência, sinal, flags, nome */
		if (nf < 5 || !strchr(f[0], ':'))
			continue;
		net_ap_t ap;
		if (decode_ssid(f[4], ap.ssid, sizeof(ap.ssid)) != 0)
			continue;
		ap.signal = atoi(f[2]);
		ap.secure = strstr(f[3], "WPA") || strstr(f[3], "WEP") || strstr(f[3], "SAE");
		int dup = -1;
		for (int i = 0; i < count; i++)
			if (!strcmp(all[i].ssid, ap.ssid))
				dup = i;
		if (dup >= 0) {	/* vários pontos de acesso da mesma rede: fica o mais forte */
			if (ap.signal > all[dup].signal)
				all[dup] = ap;
		} else if (count < (int)(sizeof(all) / sizeof(all[0]))) {
			all[count++] = ap;
		}
	}
	proc_kill(&job, 0);
	qsort(all, (size_t)count, sizeof(all[0]), ap_cmp);
	n->naps = count < NET_MAX_APS ? count : NET_MAX_APS;
	memcpy(n->aps, all, (size_t)n->naps * sizeof(all[0]));
	n->has_wifi = 1;
	n->phase = NET_WIFI_LIST;
}

/* ---- eventos do servidor ---- */

static void push_file(net_state_t *n, const net_file_t *f)
{
	int keep = n->nfiles < NET_MAX_FILES ? n->nfiles : NET_MAX_FILES - 1;
	memmove(&n->files[1], &n->files[0], (size_t)keep * sizeof(n->files[0]));
	n->files[0] = *f;
	if (n->nfiles < NET_MAX_FILES)
		n->nfiles++;
}

/* 1 se a tela mudou */
static int server_event(net_state_t *n, char *line)
{
	char *f[4];
	int nf = split_tabs(line, f, 4);
	net_file_t file;
	memset(&file, 0, sizeof(file));
	if (!strcmp(f[0], "pronto")) {
		if (n->phase == NET_CONNECTING)
			n->phase = NET_READY;
		return 1;
	}
	if (!strcmp(f[0], "inicio") && nf >= 2) {
		snprintf(n->receiving, sizeof(n->receiving), "%s", f[1]);
		return 1;
	}
	if (!strcmp(f[0], "ok") && nf >= 4) {
		file.ok = 1;
		file.bytes = atoll(f[1]);
		file.cover = !strcmp(f[2], "capa");
		snprintf(file.name, sizeof(file.name), "%s", f[3]);
		n->ok_count++;
	} else if (!strcmp(f[0], "erro") && nf >= 3) {
		snprintf(file.why, sizeof(file.why), "%s", f[1]);
		snprintf(file.name, sizeof(file.name), "%s", f[2]);
		n->err_count++;
	} else if (!strcmp(f[0], "bloqueado")) {
		set_error(n, "bloqueado");
		return 1;
	} else {
		return 0;
	}
	if (!strcmp(n->receiving, file.name))
		n->receiving[0] = 0;
	push_file(n, &file);
	return 1;
}

int net_poll(net_state_t *n)
{
	int changed = 0, st = 0;
	long long t = now_ms();

	if (n->phase == NET_CONNECTING || n->phase == NET_SCANNING) {
		if (t >= next_tick) {
			next_tick = t + TICK_MS;
			n->tick++;
			changed = 1;
		}
		if (job.pid > 0) {
			proc_read(&job);
			if (proc_exited(&job, &st)) {
				if (n->phase == NET_SCANNING)
					scan_done(n);
				else
					up_done(n, st);
				return 1;
			}
			if (t >= job.deadline) {
				set_error(n, "tempo");
				return 1;
			}
			return changed;
		}
	}

	if (server.pid > 0) {
		char line[512];
		proc_read(&server);
		while (server.pid > 0 && proc_line(&server, line, sizeof(line)))
			changed |= server_event(n, line);
		if (server.pid > 0 && proc_exited(&server, &st)) {
			proc_kill(&server, 0);
			if (n->phase == NET_CONNECTING || n->phase == NET_READY) {
				set_error(n, "servidor");
				changed = 1;
			}
		} else if (n->phase == NET_CONNECTING && t >= server.deadline) {
			set_error(n, "servidor");
			changed = 1;
		}
	}
	return changed;
}

void net_stop(net_state_t *n)
{
	proc_kill(&job, 500);
	proc_kill(&server, 3000);
	if (n->phase != NET_OFF) {
		char *argv[] = { g_net_cmd, "down", g_roms, NULL };
		proc_t down;
		if (spawn(&down, argv, DOWN_TIMEOUT_MS) == 0) {
			int st;
			while (!proc_exited(&down, &st) && now_ms() < down.deadline)
				usleep(20 * 1000);
			proc_kill(&down, 0);
		}
	}
	n->phase = NET_OFF;
	sync();
}

/* ---- teclado da senha ---- */

static const char *const KB[KB_LAYERS][KB_ROWS] = {
	{ "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_.@" },
	{ "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_.@" },
	{ "!\"#$%&'()*", "+,/:;<=>?[", "\\]^`{|}~ -", "_.@1234567" },
};

char net_kb_char(int layer, int row, int col)
{
	if (layer < 0 || layer >= KB_LAYERS || row < 0 || row >= KB_ROWS || col < 0 || col >= KB_COLS)
		return 0;
	return KB[layer][row][col];
}
