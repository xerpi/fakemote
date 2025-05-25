add_executable(egc-example
    main.c
)

target_include_directories(egc-example PRIVATE
    ..
)

target_link_libraries(egc-example PRIVATE
    embedded-game-controller
)
