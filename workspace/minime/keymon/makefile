ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS = -O2 -I../libmsettings -I../platform
LDFLAGS = -L../libmsettings -lmsettings -lpthread -lrt

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -rf $(PRODUCT)
