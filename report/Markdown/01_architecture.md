# 2 总体架构设计

## 2.1 架构目标

本项目选择 MVVM 风格的分层设计，主要目标有三个：

1. View 只处理“如何显示”和“如何接收输入”，不保存玩法规则状态。
2. ViewModel 只处理“游戏如何推进”和“当前状态是什么”，不依赖 Qt 控件或绘图 API。
3. Common 层只定义两边共享的数据形状和轻量通知工具，让 View 与 ViewModel 可以通过稳定合同通信。

这个设计使得界面、规则和应用启动逻辑分别处在不同 target 中。后续无论是替换绘制方式、扩展敌人 AI，还是调整应用启动流程，都不需要牵动整个项目。

## 2.2 依赖方向

项目的依赖方向如下：

```text
main
  -> app
       -> view
       -> viewmodel
       -> common
       -> Qt6 Core / Widgets
view
  -> common
  -> Qt6 Widgets / Gui
viewmodel
  -> common
common
  -> C++ standard library only
```

`app` 层可以看到具体的 View 和 ViewModel，因为它承担组合根职责；但 `view` 和 `viewmodel` 之间没有直接依赖。两者通过 Common 中的 `GameState`、`EventNotification` 以及 App 注入的 `std::function` 命令回调完成协作。

| 层次 | 主要文件 | 对外暴露 | 不应承担的职责 |
| --- | --- | --- | --- |
| Common | `types.h`、`game_state.h`、`notification.h`、`common.h` | 基础值类型、只读游戏状态、事件通知工具 | Qt 绘制、AI、碰撞、生命周期管理 |
| ViewModel | `GameViewModel`、`GameSimulation`、`SimulationTypes.h` | `const GameState*`、命令回调、变化通知 | 直接绘制画面、读取键盘码 |
| View | `MainWindow`、`GameWidget`、`InputDefs.h` | 属性 setter、命令 setter、Qt 窗口与画布 | 修改游戏内部状态、计算伤害 |
| App | `GameApplication` | `run()` | 游戏规则、具体绘制细节 |

## 2.3 绑定方式

当前项目参考袁老师的Plane 示例，将跨层绑定拆成三类。

第一类是属性绑定：

```text
GameViewModel::get_game_state()
  -> const GameState*
  -> MainWindow::set_game_state(...)
  -> GameWidget::set_game_state(...)
  -> paintEvent 读取只读状态
```

View 持有的是 `const GameState*`，只能读，不能写。`GameState` 中包含阶段、地图、玩家、敌人、HUD、结算等绘制所需信息。

第二类是命令绑定：

```text
GameViewModel::get_move_left_command()
  -> std::function<void(bool)>
  -> MainWindow::set_move_left_command(...)
  -> GameWidget::keyPressEvent / keyReleaseEvent 调用
```

移动命令使用 `std::function<void(bool)>` 表达按下和释放；轻攻击、重攻击、跳跃、暂停、确认、重置等单次动作使用 `std::function<void()>`；Tick 使用 `std::function<void(float, std::uint64_t)>`。这样 ViewModel 不需要知道用户按的是方向键、WASD、J 还是 Z，View 也不需要知道命令内部如何修改规则状态。

第三类是通知绑定：

```text
GameViewModel::fire(kGameStateChangedEvent)
  -> EventNotification
  -> MainWindow 返回的 lambda
  -> GameWidget::update()
```

状态变化后，ViewModel 只发出“状态已变”的通知。View 收到通知后安排重绘，并在 `paintEvent` 中通过之前绑定的只读指针读取最新 `GameState`。

## 2.4 数据流设计

项目中存在两条单向数据流。

第一条是输入和时间通道：

```text
Qt key event 
  -> GameWidget
  -> std::function command
  -> GameViewModel
  -> GameSimulation
```

View 将物理输入翻译成语义命令，例如“左移动开启”“轻攻击触发”“暂停触发”。

第二条是状态通道：

```text
GameSimulation
  -> GameViewModel::sync_state_from_simulation()
  -> GameState
  -> EventTrigger 通知
  -> GameWidget::paintEvent
```

ViewModel 将内部实体状态整理为 `GameState`。View 只根据其中的 `phase`、`map`、`player`、`enemies`、`hud`、`result`、`screenMessage` 等字段绘制画面。

## 2.5 时间推进机制

游戏循环由 View 层的 `QTimer` 驱动，约每 16 ms 触发一次。触发时，`GameWidget` 根据 `QElapsedTimer` 计算真实 delta time，并将单帧推进限制在 0.1 秒以内，避免调试或卡顿后一次性推进过多逻辑。随后 `GameWidget` 调用已绑定的 `m_tickCommand(clampedDt, frameIndex)`。


## 2.6 状态边界

`GameState` 是 ViewModel 到 View 的主要状态出口。当前状态中包含：

- `GamePhase`：标题、游玩、暂停、失败、胜利等阶段。
- `MapState`：视口大小、镜头位置、街道上下边界。
- `ActorState`：玩家、敌人、Boss 的位置、绘制尺寸、血量、动作状态、朝向和可见性。
- `HudState`：玩家血量、玩家精力、Boss 血条和疲劳提示。
- `GameResultState`：用时和击败敌人数。
- `screenMessage`：标题或暂停等阶段的屏幕提示。

内部的实体列表、敌人追踪规则、Boss 触发位置、玩家移动标志、跳跃计时、攻击计时和能量消耗常量都保留在 ViewModel 内部，不向 View 暴露。这使得 View 可以保持稳定，即使后续增加更复杂的 AI、连招或关卡配置，也只需要在 `GameState` 中补充必要显示字段。
