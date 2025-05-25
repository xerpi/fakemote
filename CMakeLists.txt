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

set(ARCH -mcpu=arm926ej-s -mthumb -mthumb-interwork -mbig-endian)
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

add_subdirectory(cios-lib)
add_subdirectory(embedded-game-controller)
add_subdirectory(source)
