# devkitARM path
DEVKITARM ?=	/opt/devkitARM

# Prefix
PREFIX	=	$(DEVKITARM)/bin/arm-none-eabi-

# Executables
CC	=	$(PREFIX)gcc
AR	=	$(PREFIX)ar

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
CFLAGS	=	$(ARCH) -I. -fomit-frame-pointer -Os -Wall -D__TIME__=\"$(BUILD_TIME)\" -D__DATE__=\"$(BUILD_DATE)\" -Wno-builtin-macro-redefined
CFLAGS2	=	-Wstrict-prototypes -ffunction-sections
ARFLAGS	=	rcs

# Libraries
LIBS	=

# Target
TARGET	=	cios-lib

# Objects
OBJS	=	direct_syscalls.o	\
		ios.o			\
		log.o			\
		mem.o			\
		str_utils.o		\
		swi_mload.o		\
		syscalls.o		\
		syscalls_dbg.o		\
		timer.o			\
		tools.o			\
		tiny-printf.o


$(TARGET).a: $(OBJS)
	@echo -e " AR\t$@"
	@$(AR) $(ARFLAGS) $@ $(OBJS)

%.o: %.s
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c -x assembler-with-cpp -o $@ $<

libwbfs/%.o: libwbfs/%.c
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) $(CFLAGS2) -c -o $@ $<

%.o: %.c
	@echo -e " CC\t$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo -e "Cleaning..."
	@rm -f $(OBJS) $(TARGET).a
