# The helper plugin (psp5time.prx): a kernel module that ARK loads into a game. psp5 carries it inside its own EBOOT.PBP
# (see ../Makefile) and writes it out at every start with ARK (src/helper.c).
TARGET = psp5time
OBJS = psp5time.o awake.o

CFLAGS = -Os -G0 -Wall -Wextra -Wno-unused-parameter -fno-pic -I../src
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LDFLAGS = -nostartfiles
LIBS = -lpspsystemctrl_kernel

PSP_FW_VERSION = 660

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

psp5time.o: ../src/awake.h

awake.o: ../src/awake.c ../src/awake.h
	$(CC) $(CFLAGS) -c -o $@ ../src/awake.c
