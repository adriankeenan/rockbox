# psp_rules
export PSPDEV ?= /opt/pspdev
export PATH := $(PSPDEV)/bin:$(PATH)

MKSFOEX  ?= mksfoex
PACK_PBP ?= pack-pbp
FIXUP    ?= psp-fixup-imports

PSP_EBOOT_TITLE := Rockbox

SFO_PATH  := $(BUILDDIR)/PARAM.SFO
EBOOT_PBP := $(BUILDDIR)/EBOOT.PBP

.SECONDEXPANSION:

$(BUILDDIR)/$(BINARY): $$(OBJ) $(FIRMLIB) $(VOICESPEEXLIB) $(CORE_LIBS)
	$(call PRINTS,LD $(@F))$(CC) -o $@ -Wl,--start-group $^ -Wl,--end-group $(LDOPTS) $(GLOBAL_LDOPTS) \
	-Wl,$(LDMAP_OPT),$(BUILDDIR)/rockbox.map
	$(call PRINTS,FIXUP $(@F))$(FIXUP) $@

$(SFO_PATH):
	$(MKSFOEX) '$(PSP_EBOOT_TITLE)' $@

$(EBOOT_PBP): $(BUILDDIR)/$(BINARY) $(SFO_PATH)
	$(PACK_PBP) $@ $(SFO_PATH) NULL NULL NULL NULL NULL $(BUILDDIR)/$(BINARY) NULL

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

PSP_PACKAGES := $(EBOOT_PBP)
build: $(PSP_PACKAGES)
bin: $(PSP_PACKAGES)
