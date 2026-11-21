/*
 * snespi-upload: servidor HTTP mínimo do modo rede do snes-pi-os.
 *
 *   snespi-upload --dir /roms --pin 1234 [--port 80] [--max-mb 16]
 *
 * Serve a página de envio na raiz (http://<ip>/), que pede o PIN mostrado
 * na TV; o QR code do launcher abre /?pin=<pin>, e a página já vem com ele
 * preenchido. Rotas:
 *
 *   GET  /                    a página (qualquer um na rede pode abrir)
 *   GET  /pin?pin=<pin>       200 se o PIN confere, 403 se não
 *   POST /enviar?pin=<pin>    os arquivos (multipart/form-data); 403 sem o PIN
 *
 * Grava direto no disco: ROMs (.sfc .smc .fig .swc) em --dir e capas
 * (.jpg .jpeg .png) em --dir/capas.
 *
 * O launcher inicia este programa só enquanto a tela "Receber jogos" está
 * aberta e lê os eventos da saída padrão, um por linha, com campos
 * separados por TAB:
 *
 *   pronto <porta>
 *   inicio <nome>
 *   ok <bytes> <rom|capa> <nome>
 *   erro <motivo> <nome>          motivo: tipo, nome, grande, disco, corte
 *   bloqueado                     PIN errado vezes demais; não atende mais
 *
 * Cada conexão é atendida num processo filho (até MAX_CHILDREN de uma vez):
 * os navegadores abrem conexões de reserva e as deixam paradas, e isso
 * travaria um servidor de uma conexão só. Uma requisição por conexão.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pagina.h"	/* PAGINA_HTML, gerado de pagina.html pelo Makefile */

#define MAX_CHILDREN 4
#define HEAD_MAX 8192		/* linha de requisição + cabeçalhos */
#define PART_HEAD_MAX 4096	/* cabeçalhos de uma parte do multipart */
#define BUF_SIZE 65536
#define NAME_MAX_BYTES 200
#define IO_TIMEOUT_S 20
#define BAD_PIN_LIMIT 30
#define EXIT_BAD_PIN 3

static const char *g_dir = "/roms";
static const char *g_pin;
static int g_port = 80;
static long long g_max = 16LL * 1024 * 1024;
static volatile sig_atomic_t g_quit;

static void on_quit(int sig) { (void)sig; g_quit = 1; }
static void on_child(int sig) { (void)sig; }

/* Evento para o launcher: uma linha, numa escrita só (atômica no pipe) */
static void event(const char *fmt, ...)
{
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n > (int)sizeof(line) - 2)
		n = (int)sizeof(line) - 2;
	line[n++] = '\n';
	ssize_t w = write(STDOUT_FILENO, line, (size_t)n);
	(void)w;
}

static int write_all(int fd, const void *data, size_t n)
{
	const char *p = data;
	while (n > 0) {
		ssize_t w = write(fd, p, n);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += w;
		n -= (size_t)w;
	}
	return 0;
}

static void respond(int fd, const char *status, const char *type, const char *body, size_t len)
{
	char head[256];
	int n = snprintf(head, sizeof(head),
			 "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
			 "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
			 status, type, len);
	if (write_all(fd, head, (size_t)n) == 0 && len)
		write_all(fd, body, len);
}

static void respond_text(int fd, const char *status, const char *text)
{
	respond(fd, status, "text/plain; charset=utf-8", text, strlen(text));
}

/* ---- nome do arquivo ---- */

enum { KIND_NONE, KIND_ROM, KIND_COVER };

static int kind_of(const char *name)
{
	static const char *const rom[] = { "sfc", "smc", "fig", "swc", NULL };
	static const char *const cover[] = { "jpg", "jpeg", "png", NULL };
	const char *dot = strrchr(name, '.');
	if (!dot || dot == name)
		return KIND_NONE;
	for (int i = 0; rom[i]; i++)
		if (strcasecmp(dot + 1, rom[i]) == 0)
			return KIND_ROM;
	for (int i = 0; cover[i]; i++)
		if (strcasecmp(dot + 1, cover[i]) == 0)
			return KIND_COVER;
	return KIND_NONE;
}

/*
 * Nome seguro para gravar em --dir (FAT32): só a última parte do caminho,
 * sem caracteres de controle nem os que o FAT recusa, sem ponto ou espaço
 * no começo (nada de arquivo oculto ou ".."), até NAME_MAX_BYTES bytes
 * mantendo a extensão. Devolve 0, ou -1 se não sobrou um nome válido.
 */
static int sanitize_name(const char *in, char *dst, size_t dstsz)
{
	char out[1024];	/* o corte para NAME_MAX_BYTES vem depois, mantendo a extensão */
	const size_t outsz = sizeof(out);
	const char *base = in;
	for (const char *p = in; *p; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	while (*base == '.' || *base == ' ')
		base++;
	size_t n = 0;
	for (const unsigned char *p = (const unsigned char *)base; *p && n + 1 < outsz; p++) {
		unsigned char c = *p;
		if (c < 0x20 || c == 0x7f || strchr("\"*:<>?|", c))
			c = '_';
		out[n++] = (char)c;
	}
	out[n] = 0;
	while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '.'))
		out[--n] = 0;
	if (n == 0)
		return -1;
	if (n > NAME_MAX_BYTES) {
		char *dot = strrchr(out, '.');
		size_t extlen = dot ? strlen(dot) : 0;
		if (extlen == 0 || extlen > 8)
			return -1;
		char ext[16];
		memcpy(ext, dot, extlen + 1);
		size_t cut = NAME_MAX_BYTES - extlen;
		/* não corta no meio de um caractere UTF-8 */
		while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80)
			cut--;
		while (cut > 0 && (out[cut - 1] == ' ' || out[cut - 1] == '.'))
			cut--;
		if (cut == 0)
			return -1;
		memcpy(out + cut, ext, extlen + 1);
	}
	if (strlen(out) >= dstsz)
		return -1;
	strcpy(dst, out);
	return 0;
}

/* ---- leitura do corpo ---- */

typedef struct {
	int fd;
	long long left;		/* bytes do corpo que ainda não saíram do socket */
	size_t len;		/* bytes válidos em buf */
	unsigned char buf[BUF_SIZE];
} body_t;

/* Lê mais do socket; devolve >0, 0 no fim do corpo, -1 em erro ou buffer cheio */
static int body_fill(body_t *b)
{
	if (b->left <= 0)
		return 0;
	size_t room = sizeof(b->buf) - b->len;
	if (room == 0)
		return -1;
	if ((long long)room > b->left)
		room = (size_t)b->left;
	ssize_t r;
	do {
		r = read(b->fd, b->buf + b->len, room);
	} while (r < 0 && errno == EINTR && !g_quit);
	if (r <= 0)
		return -1;	/* conexão caiu antes do fim do corpo */
	b->len += (size_t)r;
	b->left -= r;
	return (int)r;
}

static void body_consume(body_t *b, size_t n)
{
	memmove(b->buf, b->buf + n, b->len - n);
	b->len -= n;
}

/* Garante pelo menos n bytes no buffer; 0 se conseguiu */
static int body_need(body_t *b, size_t n)
{
	while (b->len < n)
		if (body_fill(b) <= 0)
			return -1;
	return 0;
}

/* ---- gravação ---- */

typedef struct {
	int fd;			/* -1: descartando */
	int kind;
	long long bytes;
	const char *error;	/* motivo, ou NULL */
	char name[NAME_MAX_BYTES + 64];
	char tmp[1024];
} sink_t;

static void fsync_dir(const char *dir)
{
	int fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd >= 0) {
		fsync(fd);
		close(fd);
	}
}

static void sink_abort(sink_t *s, const char *why)
{
	if (s->fd >= 0) {
		close(s->fd);
		unlink(s->tmp);
		s->fd = -1;
	}
	if (!s->error)
		s->error = why;
}

static void sink_open(sink_t *s, const char *raw_name)
{
	memset(s, 0, sizeof(*s));
	s->fd = -1;
	if (sanitize_name(raw_name, s->name, sizeof(s->name)) != 0) {
		snprintf(s->name, sizeof(s->name), "?");
		s->error = "nome";
		return;
	}
	s->kind = kind_of(s->name);
	if (s->kind == KIND_NONE) {
		s->error = "tipo";
		return;
	}
	event("inicio\t%s", s->name);
	char dir[900];
	snprintf(dir, sizeof(dir), "%s/.snespi", g_dir);
	mkdir(dir, 0755);
	snprintf(s->tmp, sizeof(s->tmp), "%s/envio-%ld.tmp", dir, (long)getpid());
	s->fd = open(s->tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (s->fd < 0)
		s->error = "disco";
}

static void sink_write(sink_t *s, const unsigned char *data, size_t n)
{
	if (s->fd < 0 || n == 0)
		return;
	s->bytes += (long long)n;
	if (s->bytes > g_max) {
		sink_abort(s, "grande");
		return;
	}
	if (write_all(s->fd, data, n) != 0)
		sink_abort(s, "disco");
}

/*
 * Fim do arquivo: fsync, rename para o destino e fsync da pasta. Se a
 * energia cair no meio, fica o arquivo antigo ou nenhum, nunca meia ROM.
 */
static void sink_close(sink_t *s, int complete)
{
	if (s->fd >= 0 && !complete)
		sink_abort(s, "corte");
	if (s->fd >= 0 && s->bytes == 0)
		sink_abort(s, "corte");
	if (s->fd >= 0) {
		char dir[900], dest[1200];
		if (s->kind == KIND_COVER) {
			snprintf(dir, sizeof(dir), "%s/capas", g_dir);
			mkdir(dir, 0755);
		} else {
			snprintf(dir, sizeof(dir), "%s", g_dir);
		}
		snprintf(dest, sizeof(dest), "%s/%s", dir, s->name);
		int bad = fsync(s->fd) != 0;
		bad |= close(s->fd) != 0;
		s->fd = -1;
		if (bad || rename(s->tmp, dest) != 0) {
			unlink(s->tmp);
			s->error = "disco";
		} else {
			fsync_dir(dir);
		}
	}
	if (s->error)
		event("erro\t%s\t%s", s->error, s->name);
	else
		event("ok\t%lld\t%s\t%s", s->bytes, s->kind == KIND_ROM ? "rom" : "capa", s->name);
}

/* ---- multipart/form-data ---- */

/* Valor de um parâmetro (name="...") numa linha de cabeçalho; 0 se achou */
static int header_param(const char *head, const char *key, char *out, size_t outsz)
{
	size_t klen = strlen(key);
	for (const char *p = head; (p = strcasestr(p, key)) != NULL; p += klen) {
		if (p > head && (isalnum((unsigned char)p[-1]) || p[-1] == '*'))
			continue;	/* "filename" não vale como "name" */
		const char *v = p + klen;
		if (*v != '=')
			continue;
		v++;
		size_t n = 0;
		if (*v == '"') {
			for (v++; *v && *v != '"' && *v != '\r' && *v != '\n'; v++)
				if (n + 1 < outsz)
					out[n++] = *v;
		} else {
			for (; *v && *v != ';' && *v != '\r' && *v != '\n' && *v != ' '; v++)
				if (n + 1 < outsz)
					out[n++] = *v;
		}
		out[n] = 0;
		return 0;
	}
	return -1;
}

/*
 * Lê as partes e grava os arquivos. Acrescenta em out uma linha por
 * arquivo ("ok\tnome" ou "erro\tmotivo\tnome"). Devolve 0, ou -1 se o
 * corpo veio malformado ou cortado.
 */
static int receive_multipart(body_t *b, const char *boundary, char *out, size_t outsz)
{
	char delim[160];
	int dlen = snprintf(delim, sizeof(delim), "\r\n--%s", boundary);
	size_t olen = 0;
	out[0] = 0;

	/* primeiro delimitador: sem o \r\n da frente; o preâmbulo é ignorado */
	for (;;) {
		unsigned char *p = memmem(b->buf, b->len, delim + 2, (size_t)dlen - 2);
		if (p) {
			body_consume(b, (size_t)(p - b->buf) + (size_t)dlen - 2);
			break;
		}
		if (b->len > (size_t)dlen)	/* guarda só o fim, onde ele pode estar pela metade */
			body_consume(b, b->len - (size_t)dlen);
		if (body_fill(b) <= 0)
			return -1;
	}

	for (;;) {
		if (body_need(b, 2) != 0)
			return -1;
		if (b->buf[0] == '-' && b->buf[1] == '-')
			return 0;	/* delimitador final */
		if (b->buf[0] != '\r' || b->buf[1] != '\n')
			return -1;
		body_consume(b, 2);

		/* cabeçalhos da parte */
		unsigned char *end;
		while (!(end = memmem(b->buf, b->len, "\r\n\r\n", 4))) {
			if (b->len >= PART_HEAD_MAX || body_fill(b) <= 0)
				return -1;
		}
		size_t hlen = (size_t)(end - b->buf);
		if (hlen >= PART_HEAD_MAX)
			return -1;
		char head[PART_HEAD_MAX];
		memcpy(head, b->buf, hlen);
		head[hlen] = 0;
		for (size_t i = 0; i < hlen; i++)
			if (head[i] == 0)
				head[i] = ' ';
		body_consume(b, hlen + 4);

		char fname[1024];
		int is_file = header_param(head, "filename", fname, sizeof(fname)) == 0 && fname[0];
		sink_t sink;
		if (is_file) {
			sink_open(&sink, fname);
		} else {
			memset(&sink, 0, sizeof(sink));
			sink.fd = -1;
		}

		/* dados até o próximo delimitador */
		int complete = 0;
		for (;;) {
			unsigned char *p = memmem(b->buf, b->len, delim, (size_t)dlen);
			if (p) {
				size_t n = (size_t)(p - b->buf);
				sink_write(&sink, b->buf, n);
				body_consume(b, n + (size_t)dlen);
				complete = 1;
				break;
			}
			if (b->len >= (size_t)dlen) {	/* o fim pode ser um delimitador pela metade */
				size_t n = b->len - ((size_t)dlen - 1);
				sink_write(&sink, b->buf, n);
				body_consume(b, n);
			}
			if (body_fill(b) <= 0)
				break;
		}
		if (is_file) {
			sink_close(&sink, complete);
			int n;
			if (sink.error)
				n = snprintf(out + olen, outsz - olen, "erro\t%s\t%s\n", sink.error, sink.name);
			else
				n = snprintf(out + olen, outsz - olen, "ok\t%s\n", sink.name);
			if (n > 0 && (size_t)n < outsz - olen)
				olen += (size_t)n;
			else
				out[olen] = 0;
		}
		if (!complete)
			return -1;
	}
}

/* ---- requisição ---- */

static const char *header_value(const char *head, const char *name)
{
	size_t n = strlen(name);
	for (const char *p = head; (p = strstr(p, "\r\n")) != NULL;) {
		p += 2;
		if (strncasecmp(p, name, n) == 0 && p[n] == ':') {
			p += n + 1;
			while (*p == ' ' || *p == '\t')
				p++;
			return p;
		}
	}
	return NULL;
}

/*
 * PIN da query (?pin=1234): 1 confere, 0 errado, -1 não veio. A comparação
 * olha todos os caracteres, para o tempo de resposta não dar pista.
 */
static int check_pin(const char *query)
{
	char got[32];
	if (!query || header_param(query, "pin", got, sizeof(got)) != 0 || !got[0])
		return -1;
	size_t n = strlen(g_pin);
	unsigned diff = strlen(got) != n;
	for (size_t i = 0; i < n && got[i]; i++)
		diff |= (unsigned)(got[i] ^ g_pin[i]);
	return diff == 0;
}

/* Atende uma conexão (processo filho); devolve o código de saída */
static int serve(int fd)
{
	static body_t body;	/* grande demais para a pilha */
	body.fd = fd;

	/* cabeçalhos */
	char *end = NULL;
	while (!end) {
		if (body.len >= HEAD_MAX) {
			respond_text(fd, "431 Request Header Fields Too Large", "cabe\xC3\xA7""alho grande demais\n");
			return 0;
		}
		ssize_t r = read(fd, body.buf + body.len, HEAD_MAX - body.len);
		if (r <= 0)
			return 0;	/* conexão de reserva do navegador, ou tempo esgotado */
		body.len += (size_t)r;
		end = memmem(body.buf, body.len, "\r\n\r\n", 4);
	}
	size_t hlen = (size_t)(end - (char *)body.buf);
	static char head[HEAD_MAX + 1];
	memcpy(head, body.buf, hlen);
	head[hlen] = 0;
	for (size_t i = 0; i < hlen; i++)
		if (head[i] == 0)
			head[i] = ' ';
	body_consume(&body, hlen + 4);

	char method[8], path[512];
	if (sscanf(head, "%7s %511s", method, path) != 2) {
		respond_text(fd, "400 Bad Request", "requisi\xC3\xA7\xC3\xA3o inv\xC3\xA1lida\n");
		return 0;
	}
	char *q = strchr(path, '?');
	if (q)
		*q++ = 0;
	const int get = strcmp(method, "GET") == 0, post = strcmp(method, "POST") == 0;

	if (get && strcmp(path, "/") == 0) {
		respond(fd, "200 OK", "text/html; charset=utf-8", PAGINA_HTML, strlen(PAGINA_HTML));
		return 0;
	}
	if (!(get && strcmp(path, "/pin") == 0) && !(post && strcmp(path, "/enviar") == 0)) {
		respond_text(fd, "404 Not Found", "");
		return 0;
	}
	/* daqui para baixo, só com o PIN; cada PIN errado conta para o bloqueio */
	int pin = check_pin(q);
	if (pin != 1) {
		respond_text(fd, "403 Forbidden", "pin\n");
		return pin == 0 ? EXIT_BAD_PIN : 0;
	}
	if (get) {
		respond_text(fd, "200 OK", "ok\n");
		return 0;
	}

	const char *cl = header_value(head, "Content-Length");
	const char *ct = header_value(head, "Content-Type");
	char boundary[128] = "";
	if (ct && strncasecmp(ct, "multipart/form-data", 19) == 0) {
		char line[512];
		snprintf(line, sizeof(line), "%.*s", (int)strcspn(ct, "\r\n"), ct);
		header_param(line, "boundary", boundary, sizeof(boundary));
	}
	if (!cl || !boundary[0]) {
		respond_text(fd, "400 Bad Request", "envie os arquivos pela p\xC3\xA1gina\n");
		return 0;
	}
	long long total = atoll(cl);
	if (total <= 0) {
		respond_text(fd, "400 Bad Request", "corpo vazio\n");
		return 0;
	}
	body.left = total - (long long)body.len;
	if (body.left < 0) {
		body.len = (size_t)total;
		body.left = 0;
	}
	const char *expect = header_value(head, "Expect");
	if (expect && strncasecmp(expect, "100-continue", 12) == 0)
		write_all(fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);

	static char result[8192];
	if (receive_multipart(&body, boundary, result, sizeof(result)) != 0) {
		respond_text(fd, "400 Bad Request", result[0] ? result : "envio incompleto\n");
		return 0;
	}
	respond_text(fd, "200 OK", result);
	return 0;
}

/* Sobras de um envio interrompido (queda de energia, launcher fechado) */
static void remove_stale_tmp(void)
{
	char dir[900], path[1200];
	snprintf(dir, sizeof(dir), "%s/.snespi", g_dir);
	DIR *d = opendir(dir);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		size_t n = strlen(e->d_name);
		if (strncmp(e->d_name, "envio-", 6) == 0 && n > 4 && strcmp(e->d_name + n - 4, ".tmp") == 0) {
			snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
			unlink(path);
		}
	}
	closedir(d);
}

static int usage(void)
{
	fprintf(stderr, "uso: snespi-upload --dir PASTA --pin PIN [--port N] [--max-mb N]\n");
	return 2;
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (!strcmp(a, "--dir") && v) { g_dir = v; i++; }
		else if (!strcmp(a, "--pin") && v) { g_pin = v; i++; }
		else if (!strcmp(a, "--port") && v) { g_port = atoi(v); i++; }
		else if (!strcmp(a, "--max-mb") && v) { g_max = atoll(v) * 1024 * 1024; i++; }
		else return usage();
	}
	if (!g_pin || strlen(g_pin) < 4 || strlen(g_pin) > 16 || g_max <= 0)
		return usage();
	for (const char *p = g_pin; *p; p++)
		if (!isdigit((unsigned char)*p))
			return usage();

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));	/* sem SA_RESTART: o accept() volta com EINTR */
	sa.sa_handler = on_quit;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sa.sa_handler = on_child;
	sigaction(SIGCHLD, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);

	int ls = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((uint16_t)g_port);
	if (ls < 0 || setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0 ||
	    bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(ls, 16) != 0) {
		fprintf(stderr, "snespi-upload: porta %d: %s\n", g_port, strerror(errno));
		return 1;
	}
	remove_stale_tmp();
	event("pronto\t%d", g_port);

	pid_t kids[MAX_CHILDREN] = { 0 };
	int nkids = 0, bad_pins = 0, blocked = 0;
	while (!g_quit) {
		/* recolhe os filhos; com todos ocupados, espera um terminar */
		for (;;) {
			int st;
			pid_t p = waitpid(-1, &st, nkids >= MAX_CHILDREN ? 0 : WNOHANG);
			if (p <= 0) {
				if (p < 0 && errno == EINTR && !g_quit && nkids >= MAX_CHILDREN)
					continue;
				break;
			}
			for (int i = 0; i < MAX_CHILDREN; i++)
				if (kids[i] == p) {
					kids[i] = 0;
					nkids--;
				}
			if (WIFEXITED(st) && WEXITSTATUS(st) == EXIT_BAD_PIN && !blocked &&
			    ++bad_pins >= BAD_PIN_LIMIT) {
				blocked = 1;
				event("bloqueado");
			}
		}
		if (g_quit)
			break;
		int fd = accept(ls, NULL, NULL);
		if (fd < 0)
			continue;	/* EINTR: sinal de saída ou filho terminou */
		if (blocked) {	/* muitas tentativas de PIN: só reabrindo o modo rede */
			close(fd);
			continue;
		}
		struct timeval tv = { IO_TIMEOUT_S, 0 };
		setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		pid_t pid = fork();
		if (pid == 0) {
			signal(SIGTERM, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			close(ls);
			int rc = serve(fd);
			shutdown(fd, SHUT_WR);
			/* espera o cliente fechar, para a resposta não se perder num RST */
			char sink[512];
			struct timeval tv2 = { 2, 0 };
			setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv2, sizeof(tv2));
			for (int i = 0; i < 64 && read(fd, sink, sizeof(sink)) > 0; i++)
				;
			close(fd);
			_exit(rc);
		}
		if (pid > 0)
			for (int i = 0; i < MAX_CHILDREN; i++)
				if (!kids[i]) {
					kids[i] = pid;
					nkids++;
					break;
				}
		close(fd);
	}

	/* encerra os envios em andamento e apaga os temporários deles */
	for (int i = 0; i < MAX_CHILDREN; i++)
		if (kids[i]) {
			kill(kids[i], SIGTERM);
			waitpid(kids[i], NULL, 0);
		}
	remove_stale_tmp();
	close(ls);
	return 0;
}
