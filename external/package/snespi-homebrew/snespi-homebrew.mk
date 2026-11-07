################################################################################
#
# snespi-homebrew
#
################################################################################

# Free test games for the ROMS partition: OpenSNES examples (MIT)
SNESPI_HOMEBREW_VERSION = 0.54.0
SNESPI_HOMEBREW_SOURCE = opensnes-examples_v$(SNESPI_HOMEBREW_VERSION).zip
SNESPI_HOMEBREW_SITE = https://github.com/k0b3n4irb/opensnes/releases/download/v$(SNESPI_HOMEBREW_VERSION)
SNESPI_HOMEBREW_LICENSE = MIT
SNESPI_HOMEBREW_LICENSE_FILES = opensnes-examples_v$(SNESPI_HOMEBREW_VERSION)/LICENSE
SNESPI_HOMEBREW_INSTALL_TARGET = NO
SNESPI_HOMEBREW_INSTALL_IMAGES = YES

define SNESPI_HOMEBREW_EXTRACT_CMDS
	$(UNZIP) -q -d $(@D) $(SNESPI_HOMEBREW_DL_DIR)/$(SNESPI_HOMEBREW_SOURCE)
endef

SNESPI_HOMEBREW_BIN = $(@D)/opensnes-examples_v$(SNESPI_HOMEBREW_VERSION)/examples/bin
SNESPI_HOMEBREW_OUT = $(BINARIES_DIR)/roms-extra

# post-image.sh copies everything in roms-extra/ to the ROMS partition
define SNESPI_HOMEBREW_INSTALL_IMAGES_CMDS
	rm -rf $(SNESPI_HOMEBREW_OUT)
	$(INSTALL) -D -m 0644 $(SNESPI_HOMEBREW_BIN)/breakout.sfc \
		"$(SNESPI_HOMEBREW_OUT)/Teste - Breakout (OpenSNES).sfc"
	$(INSTALL) -D -m 0644 $(SNESPI_HOMEBREW_BIN)/controller.sfc \
		"$(SNESPI_HOMEBREW_OUT)/Teste - Controles (OpenSNES).sfc"
	$(INSTALL) -D -m 0644 \
		$(@D)/opensnes-examples_v$(SNESPI_HOMEBREW_VERSION)/LICENSE \
		$(SNESPI_HOMEBREW_OUT)/LICENCA-OpenSNES.txt
endef

$(eval $(generic-package))
