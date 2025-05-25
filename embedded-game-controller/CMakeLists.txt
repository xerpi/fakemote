find_package(PkgConfig)
pkg_check_modules(LIBUSB IMPORTED_TARGET libusb-1.0)

option(BUILD_EXAMPLE "Build example program" OFF)

if(CMAKE_SYSTEM_NAME MATCHES "Starlet")
    set(PLATFORM_SOURCES
        backends/ogc.c
        backends/ogc_arm.c
    )
elseif(LIBUSB_FOUND)
    # Use the libusb backend
    set(PLATFORM_SOURCES
        backends/libusb.c
    )
    list(APPEND EXTRA_LIBS PkgConfig::LIBUSB)
else()
    message(FATAL_ERROR "No available platform backend")
endif()

add_library(embedded-game-controller STATIC
    ${PLATFORM_SOURCES}
    egc.c
    usb_drivers/dragonrise.c
    usb_drivers/sony_ds3.c
    usb_drivers/sony_ds4.c
)

target_include_directories(embedded-game-controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_options(embedded-game-controller PRIVATE
    ${EXTRA_LDFLAGS}
)

target_link_libraries(embedded-game-controller PRIVATE
    ${EXTRA_LIBS}
    gcc
)

if(BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
