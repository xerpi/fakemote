add_library(embedded-game-controller STATIC
    backends/ogc.c
    backends/ogc_arm.c
    egc.c
    usb_drivers/dragonrise.c
    usb_drivers/sony_ds3.c
    usb_drivers/sony_ds4.c
)

target_include_directories(embedded-game-controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_options(embedded-game-controller PRIVATE
    -nostdlib
)

target_link_libraries(embedded-game-controller PRIVATE
    cios-lib
    gcc
)

