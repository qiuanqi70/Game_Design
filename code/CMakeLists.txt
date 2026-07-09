add_library(alleyfist_common INTERFACE)
target_include_directories(alleyfist_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(alleyfist_common INTERFACE cxx_std_17)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# 先添加子目录，让 library target 可用
add_subdirectory(viewmodel)
add_subdirectory(view)

add_executable(AlleyFist
    main.cpp
)
qt_standard_project_setup()
target_link_libraries(AlleyFist
    PRIVATE
        alleyfist_common
        alleyfist_view
        alleyfist_viewmodel
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
)
