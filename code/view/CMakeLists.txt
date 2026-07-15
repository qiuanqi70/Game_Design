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
)

if(WIN32)
    target_link_libraries(alleyfist_view PRIVATE winmm)
endif()

if(BUILD_TESTING)
    add_executable(alleyfist_view_smoke_test
        ViewSmokeTest.cpp
    )

    target_link_libraries(alleyfist_view_smoke_test
        PRIVATE
            alleyfist_view
            Qt6::Test
    )
    qt_import_plugins(alleyfist_view_smoke_test
        INCLUDE Qt6::QOffscreenIntegrationPlugin
    )

    add_test(NAME alleyfist_view_smoke_test COMMAND alleyfist_view_smoke_test)
    set_tests_properties(alleyfist_view_smoke_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
