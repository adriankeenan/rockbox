# psp_rules
export PSPDEV ?= /opt/pspdev
export PATH := $(PSPDEV)/bin:$(PATH)

MKSFOEX  ?= mksfoex
PACK_PBP ?= pack-pbp
FIXUP    ?= psp-fixup-imports

PSP_EBOOT_TITLE := Rockbox

SFO_PATH  := $(BUILDDIR)/PARAM.SFO
EBOOT_PBP := $(BUILDDIR)/EBOOT.PBP

# XMB artwork: ICON0 is the 144x80 entry icon in the Game list, PIC1 the
# 480x272 wallpaper shown while the entry is highlighted. Both are checked
# in pre-rendered; res/make-art.py regenerates them from the official
# docs/logo/*.svg artwork (needs ImageMagick + Pillow, so it is not wired
# into the build).
PSP_RESDIR := $(ROOTDIR)/packaging/psp/res
PSP_ICON0  := $(PSP_RESDIR)/ICON0.PNG
PSP_PIC1   := $(PSP_RESDIR)/PIC1.PNG

.SECONDEXPANSION:

$(BUILDDIR)/$(BINARY): $$(OBJ) $(FIRMLIB) $(VOICESPEEXLIB) $(CORE_LIBS)
	$(call PRINTS,LD $(@F))$(CC) -o $@ -Wl,--start-group $^ -Wl,--end-group $(LDOPTS) $(GLOBAL_LDOPTS) \
	-Wl,$(LDMAP_OPT),$(BUILDDIR)/rockbox.map
	$(call PRINTS,FIXUP $(@F))$(FIXUP) $@

$(SFO_PATH):
	$(MKSFOEX) '$(PSP_EBOOT_TITLE)' $@

# pack-pbp argument order is fixed:
#   PARAM.SFO ICON0.PNG ICON1.PMF PIC0.PNG PIC1.PNG SND0.AT3 DATA.PSP DATA.PSAR
$(EBOOT_PBP): $(BUILDDIR)/$(BINARY) $(SFO_PATH) $(PSP_ICON0) $(PSP_PIC1)
	$(PACK_PBP) $@ $(SFO_PATH) $(PSP_ICON0) NULL NULL $(PSP_PIC1) NULL \
		$(BUILDDIR)/$(BINARY) NULL

# --- Codec loading (CONFIG_BINFMT == BINFMT_ROCK) ---
# PSPSDK has no dlopen()-equivalent, so codecs are linked at a fixed
# address and loaded with a raw read() (see lc-psp.c), same as any
# PLATFORM_NATIVE target. That address is wherever the linker actually
# placed apps/codecs.c's `codecbuf` array in the ALREADY-linked main
# binary -- PSP has no ASLR, so this is fully deterministic -- discovered
# here via nm and fed into apps/plugins/plugin.lds's PSP branch.
PSP_CODEC_ADDR_HDR := $(BUILDDIR)/psp-codec-addr.h

$(PSP_CODEC_ADDR_HDR): $(BUILDDIR)/$(BINARY)
	$(call PRINTS,NM $(@F))
	$(SILENT)addr=$$($(PSPDEV)/bin/psp-nm $(BUILDDIR)/$(BINARY) | \
		awk '$$3 == "codecbuf" { print "0x"$$1 }'); \
	if [ -z "$$addr" ]; then \
		echo "error: codecbuf symbol not found in $(BUILDDIR)/$(BINARY)" >&2; \
		exit 1; \
	fi; \
	echo "#define PSP_CODEC_ORIGIN $$addr" > $@

CONFIGFILE := $(FIRMDIR)/export/config/psp.h
CODEC_LDS := $(APPSDIR)/plugins/plugin.lds
CODECLINK_LDS := $(CODECDIR)/codec.link

$(CODECLINK_LDS): $(CODEC_LDS) $(CONFIGFILE) $(PSP_CODEC_ADDR_HDR)
	$(call PRINTS,PP $(@F))
	$(SILENT)mkdir -p $(dir $@)
	$(SILENT)$(CC) $(PPCFLAGS) -DCODEC -include $(PSP_CODEC_ADDR_HDR) \
		-E -P -x c -include config.h $< -o $@

CODECLDFLAGS = -T$(CODECLINK_LDS) -Wl,--gc-sections \
	-Wl,$(LDMAP_OPT),$(CODECDIR)/$*.map $(GLOBAL_LDOPTS)

# codecs.make's own "$(CODECS): ... $(CODECLINK_LDS)" line was parsed
# before this file was included, when CODECLINK_LDS was still unset (its
# real definition above is gated "ifndef APP_TYPE" in codecs.make, and
# PSP is an APP_TYPE build) -- so that prerequisite expanded to nothing,
# and Make has no idea $(CODECS) needs codec.link built first. Re-add it
# here, now that CODECLINK_LDS actually has a value.
$(CODECS): $(CODECLINK_LDS)

# --- Plugin loading (CONFIG_BINFMT == BINFMT_ROCK) ---
# Exactly the codec scheme above, against apps/plugin.c's `pluginbuf`
# instead of `codecbuf`. plugins.make only defines PLUGIN_LDS/PLUGINLINK_LDS
# and the fixed-address link flags for non-APP_TYPE builds (PSP is an
# APP_TYPE build), and its APP_TYPE branch assumes dlopen-style shared
# objects, so both are defined here instead. This file is included after
# plugins.make (tools/root.make), so these assignments win.
PSP_PLUGIN_ADDR_HDR := $(BUILDDIR)/psp-plugin-addr.h

$(PSP_PLUGIN_ADDR_HDR): $(BUILDDIR)/$(BINARY)
	$(call PRINTS,NM $(@F))
	$(SILENT)addr=$$($(PSPDEV)/bin/psp-nm $(BUILDDIR)/$(BINARY) | \
		awk '$$3 == "pluginbuf" { print "0x"$$1 }'); \
	if [ -z "$$addr" ]; then \
		echo "error: pluginbuf symbol not found in $(BUILDDIR)/$(BINARY)" >&2; \
		exit 1; \
	fi; \
	echo "#define PSP_PLUGIN_ORIGIN $$addr" > $@

PLUGIN_LDS := $(APPSDIR)/plugins/plugin.lds
PLUGINLINK_LDS := $(BUILDDIR)/apps/plugins/plugin.link

# -DPLUGIN, matching plugins.make's own rule -- without it plugin.lds lays
# out a codec rather than a plugin.
$(PLUGINLINK_LDS): $(PLUGIN_LDS) $(CONFIGFILE) $(PSP_PLUGIN_ADDR_HDR)
	$(call PRINTS,PP $(@F))
	$(SILENT)mkdir -p $(dir $@)
	$(SILENT)$(CC) $(PPCFLAGS) -DPLUGIN -include $(PSP_PLUGIN_ADDR_HDR) \
		-E -P -x c -include config.h $< -o $@

PLUGINLDFLAGS = -T$(PLUGINLINK_LDS) -Wl,--gc-sections \
	-Wl,$(LDMAP_OPT),$*.map $(GLOBAL_LDOPTS)

# Same staleness as $(CODECS) above: plugins.make's own
# "$(ROCKS): ... $(PLUGINLINK_LDS)" was parsed while PLUGINLINK_LDS was
# still unset, so that prerequisite expanded to nothing.
$(ROCKS): $(PLUGINLINK_LDS)

PSP_PACKAGES := $(EBOOT_PBP)
build: $(PSP_PACKAGES)
bin: $(PSP_PACKAGES)
