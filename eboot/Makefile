TARGET = hen
OBJS = main.o sceUtility_private.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = TN Homebrew Enabler
PSP_EBOOT_ICON = ICON0.PNG

BUILD_PRX = 1
ENCRYPT = 1

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak