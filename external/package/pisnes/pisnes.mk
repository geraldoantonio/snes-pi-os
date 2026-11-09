################################################################################
#
# pisnes
#
################################################################################

PISNES_VERSION = a5b427f255f922cfa65e29c4285b3d565a4c9044
PISNES_SITE = $(call github,RetroPie,pisnes,$(PISNES_VERSION))
PISNES_LICENSE = Snes9x (non-commercial)
PISNES_LICENSE_FILES = copyright.h
PISNES_DEPENDENCIES = \
	host-pkgconf \
	rpi-userland \
	sdl \
	alsa-lib \
	libglib2 \
	jpeg \
	zlib

# The Makefile names the C++ compiler CCC (not CXX) and only reads
# CFLAGS. Pass everything on the command line to override its defaults.
#
# _FORTIFY_SOURCE is turned off: this 2017-era code was always built
# without it (RetroPie, Raspbian), and glibc aborts at startup on
# patterns it flags ("buffer overflow detected"). Patch 0005 fixes the
# one hit on the Pi; disabling it keeps other untested spots from
# aborting the emulator.
PISNES_MAKE_OPTS = \
	CC="$(TARGET_CC)" \
	CCC="$(TARGET_CXX)" \
	CFLAGS="$(TARGET_CFLAGS) -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)"

define PISNES_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) $(PISNES_MAKE_OPTS) snes9x snes9x.gui
endef

# snes9x reads snes9x.cfg from its own directory, and the GUI looks for
# roms/, skins/ and preview/ there too. Keep everything together in
# /usr/share/pisnes, with roms/ pointing to the /roms partition.
# The board overlay replaces snes9x.cfg (keyboard remapped).
define PISNES_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/snes9x $(TARGET_DIR)/usr/share/pisnes/snes9x
	$(INSTALL) -D -m 0755 $(@D)/snes9x.gui $(TARGET_DIR)/usr/share/pisnes/snes9x.gui
	$(INSTALL) -D -m 0644 $(@D)/snes9x.cfg.template \
		$(TARGET_DIR)/usr/share/pisnes/snes9x.cfg
	$(INSTALL) -D -m 0644 $(@D)/skins/rpimenu16.bmp \
		$(TARGET_DIR)/usr/share/pisnes/skins/rpimenu16.bmp
	mkdir -p $(TARGET_DIR)/usr/share/pisnes/preview
	ln -sfn /roms $(TARGET_DIR)/usr/share/pisnes/roms
endef

$(eval $(generic-package))
