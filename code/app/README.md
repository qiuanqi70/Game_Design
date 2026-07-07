# App Layer

C 负责这里。

App 层只做组装和生命周期管理：

- 创建 View 和 ViewModel。
- 把 View 产生的 `alleyfist::GameCommand` 转发给 ViewModel。
- 把 ViewModel 暴露的 `alleyfist::GameSnapshot` 提供给 View 绘制。
- 监听 ViewModel 的 `alleyfist::ChangeCallback`，通知 View 重绘。

App 不写游戏规则，也不直接绘制画面。
