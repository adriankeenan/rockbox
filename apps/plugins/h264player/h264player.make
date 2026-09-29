#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

H264SRCDIR := $(APPSDIR)/plugins/h264player
H264BUILDDIR := $(BUILDDIR)/apps/plugins/h264player

ROCKS += $(H264BUILDDIR)/h264player.rock

H264_SRC := $(call preprocess, $(H264SRCDIR)/SOURCES)
H264_OBJ := $(call c2obj, $(H264_SRC))

# add source files to OTHER_SRC to get automatic dependencies
OTHER_SRC += $(H264_SRC)

# Set '-fgnu89-inline' if supported (GCCVER >= 4.1.3, GCCNUM > 401)
ifeq ($(shell expr $(GCCNUM) \> 401),1)
    H264CFLAGS = $(PLUGINFLAGS) -fgnu89-inline -DH264BSD_EXTERNAL_ALLOC
else
    H264CFLAGS = $(PLUGINFLAGS) -DH264BSD_EXTERNAL_ALLOC
endif

$(H264BUILDDIR)/h264player.rock: $(H264_OBJ) $(CODECDIR)/libmad-mpeg.a

$(H264BUILDDIR)/%.o: $(H264SRCDIR)/%.c $(H264SRCDIR)/h264player.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(H264CFLAGS) -c $< -o $@

# AAC: libfaad and the MDCT/FFT library, built from the codec library sources
# with the codec flags (as libmad-mpeg is for mpegplayer) but without the
# codec API: faad_glue.c supplies what they expect.
H264FAAD_DIR := $(RBCODECLIB_DIR)/codecs/libfaad
H264CLIB_DIR := $(RBCODECLIB_DIR)/codecs/lib
H264FAAD_SRC := $(call preprocess, $(H264FAAD_DIR)/SOURCES)
H264CLIB_SRC := $(addprefix $(H264CLIB_DIR)/,mdct.c fft-ffmpeg.c mdct_lookup.c)
H264FAAD_OBJ := $(patsubst $(H264FAAD_DIR)/%.c,$(H264BUILDDIR)/libfaad/%.o,$(H264FAAD_SRC)) \
                $(patsubst $(H264CLIB_DIR)/%.c,$(H264BUILDDIR)/libfaad/lib/%.o,$(H264CLIB_SRC)) \
                $(H264BUILDDIR)/aac_dec.o $(H264BUILDDIR)/faad_glue.o

OTHER_SRC += $(H264FAAD_SRC) $(H264CLIB_SRC)
H264FAADFLAGS = $(CODECFLAGS) -Dcodec_malloc=h264_faad_malloc -Dcodec_calloc=h264_faad_calloc -Dcodec_realloc=h264_faad_realloc -Dcodec_free=h264_faad_free -Dcodec_strlen=h264_faad_strlen -Dqsort=h264_faad_qsort -I$(H264FAAD_DIR) -I$(H264CLIB_DIR) -I$(H264SRCDIR) -O2 -fgnu89-inline

$(H264BUILDDIR)/h264player.rock: $(H264FAAD_OBJ)

$(H264BUILDDIR)/libfaad/%.o: $(H264FAAD_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(H264FAADFLAGS) -c $< -o $@

$(H264BUILDDIR)/libfaad/lib/%.o: $(H264CLIB_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(H264FAADFLAGS) -c $< -o $@

$(H264BUILDDIR)/aac_dec.o $(H264BUILDDIR)/faad_glue.o: $(H264BUILDDIR)/%.o: $(H264SRCDIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(H264FAADFLAGS) -c $< -o $@
