#!/bin/sh
# Emulador falso para testar o launcher no computador: finge rodar o jogo
# por alguns segundos e grava um save, como o PiSNES faria ao sair.
#   SNESPI_FAKE_RC=1  simula o emulador saindo com erro
rom="$1"
echo "fake-emu: jogando $rom" >&2
sleep "${SNESPI_FAKE_SECS:-2}"
stem=$(basename "$rom")
stem=${stem%.*}
mkdir -p "$HOME/.snes96_snapshots"
printf 'save falso\n' > "$HOME/.snes96_snapshots/$stem.srm"
exit "${SNESPI_FAKE_RC:-0}"
