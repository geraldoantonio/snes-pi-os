################################################################################
#
# pisnes
#
################################################################################

PISNES_VERSION = a5b427f255f922cfa65e29c4285b3d565a4c9044
PISNES_SITE = $(call github,RetroPie,pisnes,$(PISNES_VERSION))
PISNES_LICENSE = Snes9x (uso não comercial)
PISNES_LICENSE_FILES = copyright.h
PISNES_DEPENDENCIES = \
	host-pkgconf \
	rpi-userland \
	sdl \
	alsa-lib \
	libglib2 \
	jpeg \
	zlib

# O Makefile chama o compilador C++ de CCC (não CXX) e só lê CFLAGS.
# Passamos tudo pela linha de comando para sobrescrever os valores fixos.
PISNES_MAKE_OPTS = \
	CC="$(TARGET_CC)" \
	CCC="$(TARGET_CXX)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)"

define PISNES_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) $(PISNES_MAKE_OPTS) snes9x snes9x.gui
endef

# O snes9x lê snes9x.cfg da pasta do próprio binário, e o GUI procura
# roms/, skins/ e preview/ nessa mesma pasta. Por isso tudo fica junto
# em /usr/share/pisnes, com roms/ apontando para a partição /roms.
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
