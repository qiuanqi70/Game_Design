add_library(alleyfist_common INTERFACE)
target_include_directories(alleyfist_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(alleyfist_common INTERFACE cxx_std_17)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

add_executable(AlleyFist
    main.cpp
)
qt_standard_project_setup()
target_link_libraries(AlleyFist PRIVATE alleyfist_common Qt6::Core Qt6::Widgets Qt6::Gui)

add_subdirectory(view)
add_subdirectory(viewmodel)
