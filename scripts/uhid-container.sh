#!/bin/sh
# Testa, num kernel Linux dentro do QEMU, como o Linux enxerga o controle
# arcade: o descritor HID do firmware entra pelo uhid e o teste
# (firmware/arcade/tests/test_uhid.c) confere o joystick criado e os eventos.
#
# Uso (ou "make linux-test" em firmware/arcade):
#   docker run --rm -v "$PWD":/src:ro debian:12 sh /src/scripts/uhid-container.sh
#
# Usa o kernel do Debian 12 (não o da imagem do Pi): cobre o driver HID
# genérico do Linux, não o USB nem a configuração do kernel do snes-pi-os.
set -eu

arch="$(dpkg --print-architecture)"
case "$arch" in
arm64) qpkg=qemu-system-arm; qemu="qemu-system-aarch64 -M virt -cpu max"; con=ttyAMA0 ;;
amd64) qpkg=qemu-system-x86; qemu="qemu-system-x86_64"; con=ttyS0 ;;
*) echo "arquitetura sem suporte: $arch" >&2; exit 1 ;;
esac

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
	gcc libc6-dev busybox-static cpio "$qpkg" >/dev/null

t=/tmp/uhid
mkdir -p "$t/root/bin" "$t/root/mod" "$t/root/dev" "$t/root/proc" "$t/root/sys"
cd "$t"
kpkg="$(apt-cache depends "linux-image-$arch" | awk '/Depends: linux-image/ { print $2; exit }')"
apt-get download -qq "$kpkg"
dpkg -x linux-image-*.deb k
kver="$(ls k/lib/modules)"

for m in hid/hid hid/hid-generic hid/uhid input/evdev input/joydev; do
	cp "k/lib/modules/$kver/kernel/drivers/$m.ko" root/mod/
done
cp /bin/busybox root/bin/
gcc -static -O2 -Wall -I/src/firmware/arcade/src -o root/test \
	/src/firmware/arcade/tests/test_uhid.c /src/firmware/arcade/src/pad.c

cat > root/init <<'INIT'
#!/bin/busybox sh
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sys /sys
/bin/busybox mount -t devtmpfs dev /dev
for m in hid hid-generic uhid evdev joydev; do
	/bin/busybox insmod /mod/$m.ko || echo "insmod $m falhou"
done
echo "kernel $(/bin/busybox uname -r)"
/test
/bin/busybox poweroff -f
INIT
chmod +x root/init
(cd root && find . | cpio -o -H newc --quiet) | gzip > initrd.gz

# shellcheck disable=SC2086
timeout 600 $qemu -m 512 -nographic -no-reboot -nic none \
	-kernel "k/boot/vmlinuz-$kver" -initrd initrd.gz \
	-append "console=$con quiet panic=-1" | tee log.txt | grep -v '^\s*$' || true
grep -q 'RESULTADO: OK' log.txt
