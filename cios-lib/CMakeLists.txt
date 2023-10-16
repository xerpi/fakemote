add_library(cios-lib STATIC
    direct_syscalls.s
    ios.c
    log.c
    mem.c
    str_utils.c
    swi_mload.c
    syscalls.s
    syscalls_dbg.c
    timer.c
    tools.s
    tiny-printf.c
)

target_include_directories(cios-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
