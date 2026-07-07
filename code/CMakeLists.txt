add_library(alleyfist_common INTERFACE)
target_include_directories(alleyfist_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(alleyfist_common INTERFACE cxx_std_17)

add_executable(AlleyFist
    main.cpp
)
target_link_libraries(AlleyFist PRIVATE alleyfist_common)
