find_program(STRIPIOS stripios REQUIRED)

add_executable(fakemote
    start.s
    main.c
    hci_state.c
    injmessage.c
    input_device.c
    button_map.c
    fake_wiimote.c
    fake_wiimote_mgr.c
    libc.c
    wiimote_crypto.c
    conf.c
)

target_include_directories(fakemote PRIVATE
    .
)

target_compile_definitions(fakemote PRIVATE
    FAKEMOTE_MAJOR=${FAKEMOTE_MAJOR}
    FAKEMOTE_MINOR=${FAKEMOTE_MINOR}
    FAKEMOTE_PATCH=${FAKEMOTE_PATCH}
    FAKEMOTE_HASH=${FAKEMOTE_HASH}
)

target_link_libraries(fakemote PRIVATE
    cios-lib
    gcc
    embedded-game-controller
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
    DEPENDS ../FAKEMOTE.app
)

add_custom_command(
    DEPENDS ${CMAKE_PROJECT_NAME}
    OUTPUT ../FAKEMOTE.app
    COMMAND ${STRIPIOS} ${CMAKE_PROJECT_NAME}.elf ../FAKEMOTE.app
)
