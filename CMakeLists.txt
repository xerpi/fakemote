cmake_minimum_required(VERSION 3.13)

set(FAKEMOTE_MAJOR 0)
set(FAKEMOTE_MINOR 5)
set(FAKEMOTE_PATCH 1)
execute_process(COMMAND
    git describe --dirty --always --exclude "*"
    OUTPUT_VARIABLE FAKEMOTE_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(
    fakemote
    VERSION ${FAKEMOTE_MAJOR}.${FAKEMOTE_MINOR}.${FAKEMOTE_PATCH}
    LANGUAGES ASM C
)

# This is a hack just to provide a more readable name to our target platform.
# We could try to add a cmake toolchain file for Starlet in devkitPro.
if(CMAKE_SYSTEM_NAME MATCHES "Generic-dkP")
    set(CMAKE_SYSTEM_NAME "Starlet")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Starlet")
    set(ARCH -mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian)
    set(EXTRA_CFLAGS -fno-builtin)
    set(EXTRA_LDFLAGS -nostdlib)
    set(EXTRA_LIBS cios-lib)
endif()

set(CMAKE_C_STANDARD 11)

add_compile_options(
    ${ARCH}
    -O2
    -g3
    -ffreestanding
    -fomit-frame-pointer
    -ffunction-sections
    -Wall
    ${EXTRA_CFLAGS}
)

add_subdirectory(embedded-game-controller)

if(CMAKE_SYSTEM_NAME MATCHES "Starlet")
    add_subdirectory(cios-lib)
    add_subdirectory(source)
endif()
