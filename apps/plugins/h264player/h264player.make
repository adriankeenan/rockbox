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
    H264CFLAGS = $(PLUGINFLAGS) -fgnu89-inline
else
    H264CFLAGS = $(PLUGINFLAGS)
endif

$(H264BUILDDIR)/h264player.rock: $(H264_OBJ) $(CODECDIR)/libmad-mpeg.a

$(H264BUILDDIR)/%.o: $(H264SRCDIR)/%.c $(H264SRCDIR)/h264player.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(H264CFLAGS) -c $< -o $@
