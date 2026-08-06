RG35XXPRO_DIR=$(ROOTDIR)/packaging/rg35xxpro

PORT_BUILD_DIR=portdir
PORT_STAGE_DIR=$(PORT_BUILD_DIR)/stage
PORT_ROOT=$(PORT_BUILD_DIR)/ports
PORT_APP_DIR=$(PORT_ROOT)/rockbox
PORT_ZIP_NAME=rockbox-rg35xxpro.zip

portclean:
	rm -rf $(PORT_BUILD_DIR) $(PORT_ZIP_NAME)

# Lays out a KNULLI "port": everything under roms/ports on the SHARE
# partition, with a launcher script that EmulationStation picks up.
port: portclean build
	mkdir -p $(PORT_APP_DIR)/.rockbox

	$(MAKE) PREFIX=$(PORT_STAGE_DIR) install

	# An application install produces bin/ + lib/rockbox/ + share/rockbox/;
	# flatten that into the single .rockbox directory the binary looks for
	mv $(PORT_STAGE_DIR)/bin/rockbox $(PORT_APP_DIR)/rockbox
	cp -r $(PORT_STAGE_DIR)/lib/rockbox/* $(PORT_APP_DIR)/.rockbox
	cp -r $(PORT_STAGE_DIR)/share/rockbox/* $(PORT_APP_DIR)/.rockbox
	rm -rf $(PORT_STAGE_DIR)

	cp $(RG35XXPRO_DIR)/rockbox.sh $(PORT_ROOT)/rockbox.sh
	cp $(RG35XXPRO_DIR)/rockbox-icon.png $(PORT_ROOT)/rockbox-icon.png

	# gamelist.xml is shared by every port under roms/ports, so it isn't
	# safe to drop in under that name -- it would silently destroy any
	# entries other installed ports have already added to it. Ship it
	# under a different name instead: if roms/ports/gamelist.xml doesn't
	# exist yet, rename this to gamelist.xml; if it does, merge the <game>
	# block into it by hand.
	cp $(RG35XXPRO_DIR)/gamelist.xml $(PORT_ROOT)/rockbox-gamelist-entry.xml

	chmod +x $(PORT_APP_DIR)/rockbox
	chmod +x $(PORT_ROOT)/rockbox.sh

# Unpacks into /userdata/roms/ giving roms/ports/rockbox.sh and
# roms/ports/rockbox/
port-zip: port
	cd $(PORT_BUILD_DIR) && zip -9 -q -r ../$(PORT_ZIP_NAME) ports
