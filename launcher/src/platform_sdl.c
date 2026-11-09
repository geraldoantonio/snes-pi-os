/*
 * Plataforma SDL2 para testar o launcher no computador (macOS/Linux):
 * janela 1280x720 redimensionável, teclado e controles reconhecidos pelo SDL.
 * Fechar a janela encerra o launcher.
 */
#include "platform.h"

#include <SDL.h>
#include <signal.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static surface_t *back;
static SDL_GameController *pads[4];

surface_t *video_open(int w, int h)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
		SDL_Log("SDL_Init: %s", SDL_GetError());
		return NULL;
	}
	win = SDL_CreateWindow("snespi-launcher (teste)", SDL_WINDOWPOS_CENTERED,
			       SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_RESIZABLE);
	ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
	tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565,
				      SDL_TEXTUREACCESS_STREAMING, w, h) : NULL;
	if (!tex) {
		SDL_Log("SDL: %s", SDL_GetError());
		return NULL;
	}
	SDL_RenderSetLogicalSize(ren, w, h);
	back = gfx_surface_new(w, h);
	return back;
}

void video_present(void)
{
	SDL_UpdateTexture(tex, NULL, back->px, back->stride * 2);
	SDL_RenderClear(ren);
	SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}

void video_suspend(void) {}

int video_resume(void)
{
	return 0;
}

void video_close(void)
{
	gfx_surface_free(back);
	back = NULL;
	if (tex) SDL_DestroyTexture(tex);
	if (ren) SDL_DestroyRenderer(ren);
	if (win) SDL_DestroyWindow(win);
	SDL_Quit();
}

void video_detach(void)
{
	video_close();
}

static void open_pads(void)
{
	for (int i = 0; i < SDL_NumJoysticks() && i < 4; i++)
		if (!pads[i] && SDL_IsGameController(i))
			pads[i] = SDL_GameControllerOpen(i);
}

int input_open(void)
{
	open_pads();
	return 0;
}

void input_close(void)
{
	for (int i = 0; i < 4; i++)
		if (pads[i]) {
			SDL_GameControllerClose(pads[i]);
			pads[i] = NULL;
		}
}

void power_watch_open(void) {}
int power_watch_pressed(void) { return 0; }
void power_watch_close(void) {}

/* no computador não desliga nada: o launcher só fecha */
int system_power(int reboot)
{
	SDL_Log("snespi-launcher: %s (simulado)", reboot ? "reiniciar" : "desligar");
	return 1;
}

void system_led(int mode) { (void)mode; }

int input_pad_count(void)
{
	int n = 0;
	for (int i = 0; i < 4; i++)
		n += pads[i] != NULL && SDL_GameControllerGetAttached(pads[i]);
	return n;
}

static int map_key(SDL_Keycode k)
{
	switch (k) {
	case SDLK_UP: return IN_UP;
	case SDLK_DOWN: return IN_DOWN;
	case SDLK_LEFT: return IN_LEFT;
	case SDLK_RIGHT: return IN_RIGHT;
	case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_d: return IN_A;
	case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_c: return IN_B;
	case SDLK_s: return IN_X;
	case SDLK_x: return IN_Y;
	case SDLK_PAGEUP: case SDLK_a: return IN_L;
	case SDLK_PAGEDOWN: case SDLK_z: return IN_R;
	case SDLK_SPACE: return IN_START;
	case SDLK_TAB: return IN_SELECT;
	case SDLK_POWER: case SDLK_F12: return IN_POWER;	/* F12 simula o botão de desligar */
	default: return IN_NONE;
	}
}

/* Como no Pi: A do Xbox (baixo) confirma, B volta, X favorita */
static int map_button(Uint8 b)
{
	switch (b) {
	case SDL_CONTROLLER_BUTTON_A: return IN_A;
	case SDL_CONTROLLER_BUTTON_B: return IN_B;
	case SDL_CONTROLLER_BUTTON_X: return IN_X;
	case SDL_CONTROLLER_BUTTON_Y: return IN_Y;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return IN_L;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return IN_R;
	case SDL_CONTROLLER_BUTTON_START: return IN_START;
	case SDL_CONTROLLER_BUTTON_BACK: return IN_SELECT;
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
	default: return IN_NONE;
	}
}

int input_poll(in_event_t *out, int timeout_ms)
{
	SDL_Event e;
	Uint32 until = SDL_GetTicks() + (Uint32)(timeout_ms > 0 ? timeout_ms : 0);
	for (;;) {
		int left = (int)(until - SDL_GetTicks());
		if (left < 0)
			left = 0;
		if (!SDL_WaitEventTimeout(&e, left))
			return 0;
		int btn = IN_NONE, pressed = 0;
		switch (e.type) {
		case SDL_QUIT:
			raise(SIGTERM);	/* o main encerra pelo sinal */
			return 0;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			if (e.key.repeat)
				break;
			btn = map_key(e.key.keysym.sym);
			pressed = e.type == SDL_KEYDOWN;
			break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			btn = map_button(e.cbutton.button);
			pressed = e.type == SDL_CONTROLLERBUTTONDOWN;
			break;
		case SDL_CONTROLLERDEVICEADDED:
			open_pads();
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
			    e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				video_present();
			break;
		default:
			break;
		}
		if (btn != IN_NONE) {
			out->button = btn;
			out->pressed = pressed;
			return 1;
		}
		if (SDL_GetTicks() >= until)
			return 0;
	}
}
