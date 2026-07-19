ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS = -O2
LDFLAGS = -lpthread -lrt

all:
	$(CC) $(TARGET).c -I../libmsettings -I../platform -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -rf $(PRODUCT)
