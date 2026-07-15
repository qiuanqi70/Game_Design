cmake_minimum_required(VERSION 3.20)

add_library(alleyfist_view STATIC
    MainWindow.cpp
    GameWidget.cpp
    SoundManager.cpp
)

target_include_directories(alleyfist_view PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(alleyfist_view
    PUBLIC
        alleyfist_common
        Qt6::Widgets
        Qt6::Gui
        winmm
)
