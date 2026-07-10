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

qt_standard_project_setup()

if(BUILD_TESTING)
    add_executable(alleyfist_simulation_test
        GameSimulationTest.cpp
    )

    target_link_libraries(alleyfist_simulation_test
        PRIVATE
            alleyfist_viewmodel
    )

    add_test(NAME alleyfist_simulation_test COMMAND alleyfist_simulation_test)
endif()
