cmake_minimum_required(VERSION 3.13)

set(FAKEMOTE_MAJOR 0)
set(FAKEMOTE_MINOR 5)
set(FAKEMOTE_PATCH 1)
execute_process(COMMAND
    git describe --dirty --always --exclude "*"
    OUTPUT_VARIABLE FAKEMOTE_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_program(STRIPIOS stripios REQUIRED)

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
    -fno-builtin
    -ffunction-sections
    -Wall
)

add_executable(fakemote
    source/start.s
    source/button_map.c
    source/main.c
    source/hci_state.c
    source/injmessage.c
    source/input_device.c
    source/fake_wiimote.c
    source/fake_wiimote_mgr.c
    source/libc.c
    source/wiimote_crypto.c
    source/conf.c
    source/usb_hid.c
    source/usb_driver_ds3.c
    source/usb_driver_ds4.c
)

target_include_directories(fakemote PRIVATE
    include
)

target_compile_definitions(fakemote PRIVATE
    FAKEMOTE_MAJOR=${FAKEMOTE_MAJOR}
    FAKEMOTE_MINOR=${FAKEMOTE_MINOR}
    FAKEMOTE_PATCH=${FAKEMOTE_PATCH}
    FAKEMOTE_HASH=${FAKEMOTE_HASH}
)

add_subdirectory(cios-lib)

target_link_libraries(fakemote PRIVATE
    cios-lib
    gcc
)

target_link_options(fakemote PRIVATE
    ${ARCH}
    -nostartfiles
    -nostdlib
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/link.ld,-Map,${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,-static
)

add_custom_target(app ALL
    DEPENDS FAKEMOTE.app
)

add_custom_command(
    DEPENDS ${CMAKE_PROJECT_NAME}
    OUTPUT FAKEMOTE.app
    COMMAND ${STRIPIOS} ${CMAKE_PROJECT_NAME}.elf FAKEMOTE.app
)
