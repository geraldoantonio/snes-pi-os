/*
 * Desligar/reiniciar e LED de status, no Pi (Linux com BusyBox).
 *
 * O desligamento passa pelo init (poweroff/reboot do BusyBox): ele roda os
 * scripts de parada, desmonta /roms e só então mata os programas. O LED
 * externo vem do overlay gpio-led (config.txt) com o nome snespi-status; o
 * kernel o apaga ao desligar, o que indica que já pode tirar da tomada.
 */
#include "platform.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define LED_DIR "/sys/class/leds/snespi-status"

static void write_file(const char *path, const char *val)
{
	FILE *fp = fopen(path, "w");
	if (!fp)
		return;
	fputs(val, fp);
	fclose(fp);
}

void system_led(int mode)
{
	switch (mode) {
	case LED_ON:
		write_file(LED_DIR "/trigger", "default-on");
		break;
	case LED_BLINK:
		write_file(LED_DIR "/trigger", "timer");
		write_file(LED_DIR "/delay_on", "120");
		write_file(LED_DIR "/delay_off", "120");
		break;
	default:
		write_file(LED_DIR "/trigger", "none");
		write_file(LED_DIR "/brightness", "0");
		break;
	}
}

int system_power(int reboot)
{
	const char *cmd = reboot ? "/sbin/reboot" : "/sbin/poweroff";
	sync();
	pid_t pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execl(cmd, cmd, (char *)NULL);
		_exit(127);
	}
	int st;
	if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0)
		return -1;
	return 0;	/* o BusyBox só avisa o init e sai; o resto acontece depois */
}
