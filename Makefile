# psp5 – made by Mille Weber
TARGET = psp5
OBJS = src/main.o src/scan.o src/sfo.o src/iso.o src/lzo.o src/folders.o src/playtime.o src/launch.o \
       src/gfx.o src/image.o src/art.o src/loader.o src/ui.o \
       src/arkcfg.o src/plugins.o src/system.o src/settings.o src/testrun.o src/sound.o src/lang.o src/config.o src/boot.o src/music.o src/umd.o src/theme.o src/osd.o src/files.o src/diacritic.o src/icons.o src/reccache.o src/info.o src/gameops.o src/popup.o src/saves.o src/home.o src/ark4stub.o src/awake.o src/helper.o src/controls.o src/wifi.o src/netstub.o

CFLAGS = -O2 -G0 -Wall -Wno-unused-parameter -Wno-stringop-truncation -Wno-format-truncation -Iplugin $(EXTRA) $(ARKFLAGS)
ASFLAGS = $(CFLAGS)

# ARK=4 bygger ARK-4-versionen: utan ARK-5:s ArkCtrl-bibliotek (finns inte i ARK-4)
ifeq ($(ARK),4)
ARKFLAGS = -DPSP5_ARK4
ARKCTRL =
else
ARKFLAGS =
ARKCTRL = -larkctrl
endif

LIBS = -lintrafont -lpng -ljpeg -lcisoread -llz4 -lz -lm \
       -lpspmp3 -lpspgu -lpspumd -lpsprtc -lpsppower -lpsputility -lpspusb -lpspusbstor -lpspaudio -lpspwlan \
       -lpspsystemctrl_user $(ARKCTRL) -lpspkubridge

BUILD_PRX = 1
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = psp5
# EFAWARE: on a PSP Go, ARK-4 starts psp5 from the internal storage with the Memory Stick as ms0: (so the games on both are found)
SFOFLAGS = -d EFAWARE=1
PSP_EBOOT_ICON = res/ICON0.PNG
PSP_EBOOT_PIC1 = res/PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# The helper plugin (plugin/psp5time.c, a kernel module that ARK loads into a game) is built first and carried inside psp5 as a C array
# (src/helper.c writes it out at every start with ARK, and adds its two lines to PLUGINS.TXT the first time).
plugin/psp5time.prx: plugin/psp5time.c plugin/exports.exp plugin/Makefile src/awake.c src/awake.h
	$(MAKE) -C plugin

plugin/psp5time_prx.h: plugin/psp5time.prx
	bin2c $< $@ psp5time_prx

src/helper.o: plugin/psp5time_prx.h

clean: plugin-clean
.PHONY: plugin-clean
plugin-clean:
	-$(MAKE) -C plugin clean
	-rm -f plugin/psp5time_prx.h
