cmake_minimum_required(VERSION 3.20)

add_library(alleyfist_viewmodel STATIC
    GameViewModel.cpp
    GameSimulation.cpp
)

target_include_directories(alleyfist_viewmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(alleyfist_viewmodel PRIVATE alleyfist_common Qt6::Core)

qt_standard_project_setup()
