cmake_minimum_required(VERSION 3.20)

add_library(alleyfist_app STATIC
    GameApplication.h
    GameApplication.cpp
)

set_target_properties(alleyfist_app PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(alleyfist_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(alleyfist_app
    PRIVATE
        alleyfist_common
        alleyfist_view
        alleyfist_viewmodel
        Qt6::Core
        Qt6::Widgets
)
