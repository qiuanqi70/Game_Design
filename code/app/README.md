# App Layer

C 负责这里。

App 层只做组装和生命周期管理：

- 创建 View 和 ViewModel。
- 用 `GameApplication` 管理 Qt 应用生命周期和主窗口启动。
- 调用 View 暴露的 `bind(...)`，把 ViewModel 作为公共契约接口传入。

App 不写游戏规则，不直接绘制画面，也不直接读取 `GameWidget` 或监听
`GameViewModel` 的具体信号。
