add_executable(alleyfist_viewmodel_test
    ViewModelTest.cpp
)

target_link_libraries(alleyfist_viewmodel_test
    PRIVATE
        alleyfist_viewmodel
        Qt6::Test
)

add_test(NAME alleyfist_viewmodel_test COMMAND alleyfist_viewmodel_test)

add_executable(alleyfist_view_test
    ViewTest.cpp
)

target_link_libraries(alleyfist_view_test
    PRIVATE
        alleyfist_view
        Qt6::Test
)

qt_import_plugins(alleyfist_view_test
    INCLUDE Qt6::QOffscreenIntegrationPlugin
)

add_test(NAME alleyfist_view_test COMMAND alleyfist_view_test)
set_tests_properties(alleyfist_view_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(alleyfist_app_test
    AppTest.cpp
)

target_link_libraries(alleyfist_app_test
    PRIVATE
        alleyfist_app
        alleyfist_view
        Qt6::Test
)

qt_import_plugins(alleyfist_app_test
    INCLUDE Qt6::QOffscreenIntegrationPlugin
)

add_test(NAME alleyfist_app_test COMMAND alleyfist_app_test)
set_tests_properties(alleyfist_app_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
