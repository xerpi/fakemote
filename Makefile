# devkitARM path
DEVKITARM ?=	/opt/devkitARM

# Prefix
PREFIX	=	$(DEVKITARM)/bin/arm-none-eabi-

# Executables
CC	=	$(PREFIX)gcc
LD	=	$(PREFIX)gcc
STRIP	=	stripios

# Date & Time
ifdef SOURCE_DATE_EPOCH
    BUILD_DATE ?= $(shell LC_ALL=C date -u -d "@$(SOURCE_DATE_EPOCH)" "+'%b %e %Y'" 2>/dev/null || LC_ALL=C date -u -r "$(SOURCE_DATE_EPOCH)" "+'%b %e %Y'" 2>/dev/null || LC_ALL=C date -u "+'%b %e %Y'")
    BUILD_TIME ?= $(shell LC_ALL=C date -u -d "@$(SOURCE_DATE_EPOCH)" "+'%T'" 2>/dev/null || LC_ALL=C date -u -r "$(SOURCE_DATE_EPOCH)" "+'%T'" 2>/dev/null || LC_ALL=C date -u "+'%T'")
else
    BUILD_DATE ?= $(shell LC_ALL=C date "+'%b %e %Y'")
    BUILD_TIME ?= $(shell LC_ALL=C date "+'%T'")
endif

# Flags
ARCH	=	-mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian
CFLAGS	=	$(ARCH) -Iinclude -Icios-lib -fomit-frame-pointer -O1 -g3 -Wall -Wstrict-prototypes -ffunction-sections -D__TIME__=\"$(BUILD_TIME)\" -D__DATE__=\"$(BUILD_DATE)\" -Wno-builtin-macro-redefined -nostdlib $(EXTRA_CFLAGS)
LDFLAGS	=	$(ARCH) -nostartfiles -nostdlib -Wl,-T,link.ld,-Map,$(TARGET).map -Wl,--gc-sections -Wl,-static

# Libraries
LIBS	=	cios-lib/cios-lib.a

# Target
TARGET	=	FAKEMOTE

# Objects
OBJS	= source/start.o source/main.o source/hci_state.o source/fake_wiimote_mgr.o source/libc.o \
	  source/wiimote_crypto.o source/conf.o source/usb_hid.o source/usb_driver_ds3.o \
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
