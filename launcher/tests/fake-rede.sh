#!/bin/sh
# Rede falsa para testar o modo rede no computador (make run): não mexe em
# nada, só responde como o snespi-rede do Pi, com o endereço desta máquina.
#   SNESPI_FAKE_REDE=sem-rede|sem-wifi|wifi-falhou|dhcp  simula o erro
#   SNESPI_FAKE_WIFI=1                                   finge um adaptador Wi-Fi
wifi="${SNESPI_FAKE_WIFI:-0}"
case "$1" in
up)
	sleep 2
	if [ -n "$SNESPI_FAKE_REDE" ]; then
		printf 'erro\t%s\t%s\n' "$SNESPI_FAKE_REDE" "$wifi"
		exit 1
	fi
	ip=$(ipconfig getifaddr en0 2>/dev/null || hostname -I 2>/dev/null | cut -d' ' -f1)
	ssid=""
	[ "$wifi" = 1 ] && [ -s "$2/wifi.txt" ] && ssid=$(sed -n 1p "$2/wifi.txt")
	printf 'ok\tfalsa0\t%s\t%s\t%s\n' "${ip:-127.0.0.1}" "$wifi" "$ssid"
	;;
scan)
	sleep 2
	printf 'bssid / frequency / signal level / flags / ssid\n'
	printf '02:00:00:00:00:01\t2437\t-48\t[WPA2-PSK-CCMP][ESS]\tRede de Casa\n'
	printf '02:00:00:00:00:02\t2412\t-66\t[WPA2-PSK-CCMP][ESS]\tFam\\xc3\\xadlia Silva\n'
	printf '02:00:00:00:00:03\t2462\t-71\t[ESS]\tCafeteria (visitantes)\n'
	printf '02:00:00:00:00:04\t2462\t-80\t[WPA2-PSK-CCMP][ESS]\tRede de Casa\n'
	printf '02:00:00:00:00:05\t2462\t-85\t[WPA2-PSK-CCMP][ESS]\t\n'
	;;
down)
	;;
esac
