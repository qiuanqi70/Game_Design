cmake_minimum_required(VERSION 3.20)

add_library(alleyfist_viewmodel STATIC
    GameViewModel.cpp
    GameSimulation.cpp
    SimulationTypes.h
)

set_target_properties(alleyfist_viewmodel PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(alleyfist_viewmodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(alleyfist_viewmodel
    PUBLIC
        alleyfist_common
)
