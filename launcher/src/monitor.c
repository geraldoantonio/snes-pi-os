/*
 * Monitor do emulador: enquanto o jogo roda, anota no emulador.log o uso
 * de CPU e o estado de cada thread do emulador, a carga, a memória, a
 * temperatura, a frequência e as mensagens novas do kernel.
 *
 * Serve para entender lentidão e travamentos sem cabo serial: uma thread
 * a 100 % indica emulação pesada demais para a CPU; uma thread parada em
 * estado D com wchan de áudio/GPU indica espera no driver.
 *
 * Só lê /proc e /sys (Linux). Fora do Linux, as leituras falham e o
 * monitor não escreve nada.
 */
#include "monitor.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_TASKS 32
/* com a depuração ligada numa sessão longa: para de anotar ao passar disto */
#define MAX_LOG_BYTES (256 * 1024)

static int logfd = -1, kmsg = -1;
static pid_t emu;
static long long t_prev, t_start;
static struct { int tid; unsigned long long ticks; } prev[MAX_TASKS];
static int nprev;

static long long mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Lê a primeira linha de um arquivo, sem o \n; "" se falhar */
static void read_line(const char *path, char *out, size_t n)
{
	out[0] = 0;
	FILE *fp = fopen(path, "r");
	if (!fp)
		return;
	if (fgets(out, (int)n, fp))
		out[strcspn(out, "\n")] = 0;
	fclose(fp);
}

static unsigned long long prev_ticks(int tid, int *found)
{
	for (int i = 0; i < nprev; i++)
		if (prev[i].tid == tid) {
			*found = 1;
			return prev[i].ticks;
		}
	*found = 0;
	return 0;
}

void monitor_start(pid_t pid, int fd)
{
	emu = pid;
	logfd = fd;
	nprev = 0;
	t_start = t_prev = mono_ms();
	/* só as mensagens do kernel a partir de agora */
	kmsg = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (kmsg >= 0)
		lseek(kmsg, 0, SEEK_END);
}

static void dump_kmsg(void)
{
	if (kmsg < 0)
		return;
	char rec[1024];
	for (int i = 0; i < 30; i++) {
		ssize_t n = read(kmsg, rec, sizeof(rec) - 1);
		if (n <= 0)
			break;	/* EAGAIN: acabou; EPIPE: registro perdido, segue */
		rec[n] = 0;
		char *msg = strchr(rec, ';');
		msg = msg ? msg + 1 : rec;
		msg[strcspn(msg, "\n")] = 0;
		dprintf(logfd, "[kernel] %s\n", msg);
	}
}

void monitor_tick(void)
{
	if (logfd < 0)
		return;
	struct stat lst;
	if (fstat(logfd, &lst) == 0 && lst.st_size > MAX_LOG_BYTES) {
		dprintf(logfd, "[monitor] log passou de %d KB: amostras suspensas at\xC3\xA9 o fim do jogo\n",
			MAX_LOG_BYTES / 1024);
		fsync(logfd);
		monitor_stop();
		return;
	}
	char path[96], buf[512];
	long long now = mono_ms();
	double dt = (now - t_prev) / 1000.0;
	long hz = sysconf(_SC_CLK_TCK);
	t_prev = now;

	char load[64], temp[32], freq[32];
	read_line("/proc/loadavg", load, sizeof(load));
	if (!load[0])
		return;	/* sem /proc: não é Linux */
	read_line("/sys/class/thermal/thermal_zone0/temp", temp, sizeof(temp));
	read_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", freq, sizeof(freq));
	long memavail = -1;
	FILE *fp = fopen("/proc/meminfo", "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp))
			if (sscanf(buf, "MemAvailable: %ld", &memavail) == 1)
				break;
		fclose(fp);
	}
	dprintf(logfd, "[monitor %llds] carga %.14s | %.1f °C | %ld MHz | livre %ld MB\n",
		(now - t_start) / 1000, load, atoi(temp) / 1000.0, atol(freq) / 1000,
		memavail / 1024);

	/* uma linha por thread: nome, estado, % de CPU no intervalo, wchan */
	snprintf(path, sizeof(path), "/proc/%d/task", (int)emu);
	DIR *d = opendir(path);
	if (d) {
		struct dirent *e;
		int n = 0;
		typeof(prev) cur;
		while ((e = readdir(d)) && n < MAX_TASKS) {
			int tid = atoi(e->d_name);
			if (tid <= 0)
				continue;
			snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", (int)emu, tid);
			read_line(path, buf, sizeof(buf));
			char *lp = strchr(buf, '('), *rp = strrchr(buf, ')');
			if (!lp || !rp)
				continue;
			char comm[20];
			snprintf(comm, sizeof(comm), "%.*s", (int)(rp - lp - 1), lp + 1);
			char state = rp[2];
			/* depois de ") ": estado é o campo 3; utime e stime, 14 e 15 */
			unsigned long long ut = 0, stt = 0;
			sscanf(rp + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &ut, &stt);
			unsigned long long ticks = ut + stt;
			int found;
			unsigned long long before = prev_ticks(tid, &found);
			double pct = found && dt > 0 ? 100.0 * (double)(ticks - before) / ((double)hz * dt) : -1;
			char wchan[64];
			snprintf(path, sizeof(path), "/proc/%d/task/%d/wchan", (int)emu, tid);
			read_line(path, wchan, sizeof(wchan));
			if (pct >= 0)
				dprintf(logfd, "  %-15s %c %5.1f%%  %s\n", comm, state, pct, wchan[0] ? wchan : "-");
			else
				dprintf(logfd, "  %-15s %c   ...   %s\n", comm, state, wchan[0] ? wchan : "-");
			cur[n].tid = tid;
			cur[n].ticks = ticks;
			n++;
		}
		closedir(d);
		memcpy(prev, cur, sizeof(cur[0]) * (size_t)n);
		nprev = n;
	}
	dump_kmsg();
	fsync(logfd);
}

void monitor_stop(void)
{
	if (logfd >= 0)
		dump_kmsg();
	if (kmsg >= 0)
		close(kmsg);
	kmsg = logfd = -1;
}
