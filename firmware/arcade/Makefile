# Controle arcade (Raspberry Pi Pico)
#
#   make test    confere a lógica (filtro das chaves e relatório) no computador
#   make uf2     compila o firmware no Docker; saída em build/
#   make linux-test   confere no Docker (QEMU) como o Linux enxerga o controle
#   make clean

CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra

test: build/test_pad
	./build/test_pad

build/test_pad: tests/test_pad.c src/pad.c src/pad.h src/pins.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc -o $@ tests/test_pad.c src/pad.c

uf2:
	@mkdir -p build
	docker run --rm -e HOST_UID=$$(id -u) -v "$(CURDIR)/../..":/src:ro \
		-v snespi-pico:/work -v "$(CURDIR)/build":/out \
		debian:12 sh /src/scripts/firmware-container.sh

linux-test:
	docker run --rm -v "$(CURDIR)/../..":/src:ro debian:12 sh /src/scripts/uhid-container.sh

clean:
	rm -rf build

.PHONY: test uf2 linux-test clean
