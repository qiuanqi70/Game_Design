# View Layer

陈棋隆负责这里。

View 层负责 Qt 窗口、定时器、键盘事件和界面绘制。

当前文件包括：

- `MainWindow.h/.cpp`：主窗口，承载 `GameWidget`，对 App 层暴露属性 setter、命令 setter 和通知闭包。
- `GameWidget.h/.cpp`：游戏画布，负责 60 FPS tick、键盘映射、世界坐标投影、背景/角色/HUD/覆盖层绘制。
- `InputDefs.h`：View 层内部输入聚合类型 `MovementIntent`。

View 只能依赖 Common 中的公共契约：

- 持有 `const alleyfist::GameState*`，每次绘制时只读状态。
- 通过 `std::function` 命令回调把输入和 tick 发送给 ViewModel。
- 通过 `EventNotification` 接收状态变化通知，并触发 `update()` 重绘。
- 根据状态对象绘制地图、角色、敌人、血条、精力条、Boss 血条、进度条和胜负界面。

View 不依赖 ViewModel，不直接修改游戏数据，不写 AI、碰撞、伤害、刷怪和推图逻辑。
