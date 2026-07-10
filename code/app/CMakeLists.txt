cmake_minimum_required(VERSION 3.20)

add_library(alleyfist_app STATIC
    GameApplication.h
    GameApplication.cpp
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

if(BUILD_TESTING)
    add_executable(alleyfist_app_smoke_test
        AppSmokeTest.cpp
    )

    target_link_libraries(alleyfist_app_smoke_test
        PRIVATE
            alleyfist_app
    )

    add_test(NAME alleyfist_app_smoke_test COMMAND alleyfist_app_smoke_test)
endif()
