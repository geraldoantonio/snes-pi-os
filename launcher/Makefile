# snespi-launcher
#
#   make                 binário para o Pi (Linux: framebuffer + evdev)
#   make preview         binário de captura para desenvolvimento (qualquer SO)
#   make shots           gera capturas de todas as telas com ROMs de teste
#   make run             abre o launcher numa janela (SDL2), com ROMs de teste e
#                        emulador falso; ARGS="..." passa opções extras
#   make install DESTDIR=... PREFIX=/usr

CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers
LDLIBS += -lm
PREFIX ?= /usr
PYTHON ?= python3

COMMON = src/gfx.c src/font.c src/rom.c src/cover.c src/prefs.c src/music.c src/ui.c src/shot.c src/main.c
HEADERS = $(wildcard src/*.h)

all: snespi-launcher

snespi-launcher: $(COMMON) src/video_fb.c src/input_evdev.c src/music_alsa.c $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON) src/video_fb.c src/input_evdev.c src/music_alsa.c \
		-lasound -lpthread $(LDLIBS)

preview: snespi-launcher-preview

snespi-launcher-preview: $(COMMON) src/platform_none.c src/music_none.c $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON) src/platform_none.c src/music_none.c $(LDLIBS)

SDL2_CONFIG ?= sdl2-config

desktop: snespi-launcher-desktop

snespi-launcher-desktop: $(COMMON) src/platform_sdl.c src/music_sdl.c $(HEADERS)
	$(CC) $(CFLAGS) $(shell $(SDL2_CONFIG) --cflags) $(LDFLAGS) -o $@ \
		$(COMMON) src/platform_sdl.c src/music_sdl.c $(shell $(SDL2_CONFIG) --libs) $(LDLIBS)

# Abre o launcher numa janela, com ROMs de teste e um emulador falso
run: snespi-launcher-desktop
	@test -d build/test-roms || $(PYTHON) -I tests/make-test-roms.py build/test-roms
	HOME=build/test-roms ./snespi-launcher-desktop --roms build/test-roms \
		--fonts fonts --emu tests/fake-emu.sh --music music/menu.wav $(ARGS)

shots: snespi-launcher-preview
	rm -rf build/test-roms build/empty-roms && mkdir -p build/shots build/empty-roms
	$(PYTHON) -I tests/make-test-roms.py build/test-roms
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/list.png --screen list --select 1
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/list-sem-capa.png --screen list --select 3
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/warn.png --screen warn --select 2
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/launch.png --screen launch --select 1
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/favoritos.png --screen list --filter favoritos --select 1
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/recentes.png --screen list --filter recentes --select 0
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/nome-2-linhas.png --screen list --select 12
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/nome-longo.png --screen list --select 13
	HOME=build/test-roms ./snespi-launcher-preview --roms build/test-roms --fonts fonts --shot build/shots/teclado.png --screen list --select 1 --pads 0
	./snespi-launcher-preview --roms build/empty-roms --fonts fonts --shot build/shots/empty.png --screen empty
	./snespi-launcher-preview --roms build/empty-roms --fonts fonts --shot build/shots/nomount.png --screen nomount
	./snespi-launcher-preview --fonts fonts --shot build/shots/abertura.png --screen splash --splash 15 --splash-msg "Iniciando o sistema"
	./snespi-launcher-preview --fonts fonts --shot build/shots/abertura-jogos.png --screen splash --splash 80 --splash-msg "Lendo os jogos · 205 de 410"

test: snespi-launcher-preview
	rm -rf build/test-roms
	$(PYTHON) -I tests/make-test-roms.py build/test-roms
	$(PYTHON) -I tests/check-headers.py ./snespi-launcher-preview build/test-roms
	$(CC) -O2 -std=gnu11 -w -o build/music-decode-test tests/music-decode-test.c src/music.c src/music_none.c -lm
	./build/music-decode-test music/menu.wav 70

install: snespi-launcher
	install -D -m 0755 snespi-launcher $(DESTDIR)$(PREFIX)/bin/snespi-launcher
	install -d $(DESTDIR)$(PREFIX)/share/snespi/fonts
	install -m 0644 fonts/*.ttf fonts/OFL-*.txt $(DESTDIR)$(PREFIX)/share/snespi/fonts/
	install -D -m 0644 music/menu.wav $(DESTDIR)$(PREFIX)/share/snespi/musica/menu.wav
	install -D -m 0644 music/LICENCA.txt $(DESTDIR)$(PREFIX)/share/snespi/musica/LICENCA.txt

clean:
	rm -rf snespi-launcher snespi-launcher-preview snespi-launcher-desktop build

.PHONY: all preview desktop run shots test install clean
