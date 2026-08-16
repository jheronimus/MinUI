###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

COMMON = ../common/utils.c ../common/api.c ../common/scaler.c ../common/menu.c ../common/keyboard.c ../../$(PLATFORM)/platform/platform.c

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(ARCH) -fomit-frame-pointer
CFLAGS  += -I. -I../common/ -I../../$(PLATFORM)/platform/ -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) -Ofast -std=gnu99
LDFLAGS	 = -ldl $(LIBS) -lmsettings -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz

PRODUCTS = build/$(PLATFORM)/wifi.elf build/$(PLATFORM)/bt.elf build/$(PLATFORM)/power.elf

all: $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) wifi.c $(COMMON) ../common/generic_wifi.c -o build/$(PLATFORM)/wifi.elf $(CFLAGS) $(LDFLAGS)
	$(CC) bt.c $(COMMON) ../common/generic_bt.c -o build/$(PLATFORM)/bt.elf $(CFLAGS) $(LDFLAGS)
	$(CC) power.c $(COMMON) -o build/$(PLATFORM)/power.elf $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCTS)

$(PREFIX)/include/msettings.h:
	cd ../../$(PLATFORM)/libmsettings && make
