# ViewModel/Simulation 逻辑测试：无需 QWidget，只依赖 QtTest 和业务层。
add_executable(alleyfist_viewmodel_test
    ViewModelTest.cpp
)

target_link_libraries(alleyfist_viewmodel_test
    PRIVATE
        alleyfist_viewmodel
        Qt6::Test
)

add_test(NAME alleyfist_viewmodel_test COMMAND alleyfist_viewmodel_test)

# View 测试：验证输入、资源、窗口转发和离屏渲染结果。
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

# CTest 强制使用 offscreen 平台，确保无显示器环境也能创建 QWidget。
add_test(NAME alleyfist_view_test COMMAND alleyfist_view_test)
set_tests_properties(alleyfist_view_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# 应用集成测试：验证组合根以及 View、ViewModel、计时器之间的完整绑定。
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

# 应用测试同样使用离屏 Qt 平台，避免弹出真实窗口。
add_test(NAME alleyfist_app_test COMMAND alleyfist_app_test)
set_tests_properties(alleyfist_app_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
