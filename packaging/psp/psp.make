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

PSP_PACKAGES := $(EBOOT_PBP)
build: $(PSP_PACKAGES)
bin: $(PSP_PACKAGES)
