# App Layer

苏易文负责这里。

App 层只做组装和生命周期管理：

- 创建 View 和 ViewModel。
- 用 `GameApplication` 管理 Qt 应用生命周期和主窗口启动。
- 在组合根中完成 properties / commands / notification 绑定。
- 用 PImpl 隐藏 Qt、`MainWindow` 和 `GameViewModel` 的具体类型，保持公开头文件轻量。

当前绑定流程包括：

- `mainWindow.set_game_state(viewModel.get_game_state())`
- `mainWindow.set_*_command(viewModel.get_*_command())`
- `viewModel.add_notification(mainWindow.get_notification())`

App 不写游戏规则，不直接绘制画面，也不直接读取 `GameWidget` 的内部状态。
