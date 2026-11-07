################################################################################
#
# snes-launcher
#
################################################################################

# Source lives in this repository (launcher/), next to the br2-external tree
SNES_LAUNCHER_VERSION = 1.0
SNES_LAUNCHER_SITE = $(BR2_EXTERNAL_SNESPI_PATH)/../launcher
SNES_LAUNCHER_SITE_METHOD = local
SNES_LAUNCHER_LICENSE = MIT, OFL-1.1 (fonts), Public Domain (stb), CC0-1.0 (menu music)
SNES_LAUNCHER_LICENSE_FILES = LICENSE fonts/OFL-IBMPlexMono.txt fonts/OFL-PixelifySans.txt \
	music/LICENCA.txt
SNES_LAUNCHER_DEPENDENCIES = alsa-lib

# -B: always rebuild, so a binary left in the source tree from a desktop
# build is never installed by mistake
define SNES_LAUNCHER_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -B -C $(@D) \
		CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		snespi-launcher
endef

define SNES_LAUNCHER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/snespi-launcher $(TARGET_DIR)/usr/bin/snespi-launcher
	$(INSTALL) -d $(TARGET_DIR)/usr/share/snespi/fonts
	$(INSTALL) -m 0644 $(@D)/fonts/*.ttf $(@D)/fonts/OFL-*.txt \
		$(TARGET_DIR)/usr/share/snespi/fonts/
	$(INSTALL) -D -m 0644 $(@D)/music/menu.wav \
		$(TARGET_DIR)/usr/share/snespi/musica/menu.wav
	$(INSTALL) -D -m 0644 $(@D)/music/LICENCA.txt \
		$(TARGET_DIR)/usr/share/snespi/musica/LICENCA.txt
endef

$(eval $(generic-package))
