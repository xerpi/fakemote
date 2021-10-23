# devkitARM path
DEVKITARM ?=	/opt/devkitARM

# Prefix
PREFIX	=	$(DEVKITARM)/bin/arm-none-eabi-

# Executables
CC	=	$(PREFIX)gcc
LD	=	$(PREFIX)gcc
STRIP	=	stripios

# Version
FAKEMOTE_MAJOR	=	0
FAKEMOTE_MINOR	=	1
FAKEMOTE_PATCH	=	1
FAKEMOTE_HASH	=	"$(shell git describe --dirty --always --exclude '*')"

# Flags
ARCH	=	-mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian
CFLAGS	=	$(ARCH) -Iinclude -Icios-lib -fomit-frame-pointer -O2 -g3 \
		-ffreestanding -fno-builtin -Wall -Wstrict-prototypes -ffunction-sections \
		-DFAKEMOTE_MAJOR=$(FAKEMOTE_MAJOR) -DFAKEMOTE_MINOR=$(FAKEMOTE_MINOR) \
		-DFAKEMOTE_PATCH=$(FAKEMOTE_PATCH) -DFAKEMOTE_HASH=\"$(FAKEMOTE_HASH)\" \
		$(EXTRA_CFLAGS)
LDFLAGS	=	$(ARCH) -nostartfiles -nostdlib -Wl,-T,link.ld,-Map,$(TARGET).map -Wl,--gc-sections -Wl,-static

# Libraries
LIBS	=	cios-lib/cios-lib.a

# Target
TARGET	=	FAKEMOTE

# Objects
OBJS	= source/start.o		\
	  source/button_map.o		\
	  source/main.o			\
	  source/hci_state.o		\
	  source/injmessage.o		\
	  source/fake_wiimote.o		\
	  source/fake_wiimote_mgr.o	\
	  source/libc.o			\
	  source/wiimote_crypto.o	\
	  source/conf.o			\
	  source/usb_hid.o		\
	  source/usb_driver_ds3.o	\
	  source/usb_driver_ds4.o


# Dependency files
DEPS	= $(OBJS:.o=.d)

$(TARGET).app: $(TARGET).elf
	@echo -e " STRIP\t$@"
	@$(STRIP) $< $@

$(TARGET).elf: $(OBJS) $(LIBS) link.ld
	@echo -e " LD\t$@"
	@$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -lgcc -o $@

%.o: %.s
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -MMD -MP -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

%.o: %.c
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

cios-lib/cios-lib.a:
	@$(MAKE) -C cios-lib

.PHONY: clean

clean:
	@echo -e "Cleaning..."
	@rm -f $(OBJS) $(DEPS) $(TARGET).app $(TARGET).elf $(TARGET).elf.orig $(TARGET).map
	@$(MAKE) -C cios-lib clean

-include $(DEPS)
