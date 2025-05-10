add_library(ogc-usb-hid STATIC
    button_map.c
    client.c
    usb_hid.c
    usb_drivers/dragonrise.c
    usb_drivers/sony_ds3.c
    usb_drivers/sony_ds4.c
)

target_include_directories(ogc-usb-hid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_options(ogc-usb-hid PRIVATE
    -nostdlib
)

target_link_libraries(ogc-usb-hid PRIVATE
    cios-lib
    gcc
)

