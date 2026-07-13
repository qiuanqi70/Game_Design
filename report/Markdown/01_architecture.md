# 2 总体架构设计

## 2.1 架构目标

本项目选择 MVVM 风格的分层设计，主要目标有三个：

1. View 只处理“如何显示”和“如何接收输入”，不保存玩法规则状态。
2. ViewModel 只处理“游戏如何推进”和“当前状态是什么”，不依赖 Qt 控件或绘图 API。
3. Common 层只定义两边共享的数据形状和接口，让 View 与 ViewModel 可以通过稳定合同通信。

这个设计使得界面、规则和应用启动逻辑分别处在不同 target 中，后续无论是替换绘制方式、扩展敌人 AI，还是调整应用启动流程，都不需要牵动整个项目。

## 2.2 依赖方向

项目的依赖方向如下：

```text
main
  -> app
       -> view
       -> viewmodel
       -> common
view
  -> common
viewmodel
  -> common
common
  -> C++ standard library only
```

`app` 层可以看到具体的 View 和 ViewModel，因为它承担组合根职责；但 `view` 和 `viewmodel` 之间没有直接依赖。两者只通过 `common/contracts.h` 中的接口互相配合。

| 层次 | 主要文件 | 对外暴露 | 不应承担的职责 |
| --- | --- | --- | --- |
| Common | `actions.h`、`snapshot.h`、`types.h`、`contracts.h` | 命令、快照、基础值类型和绑定接口 | Qt 绘制、AI、碰撞、生命周期管理 |
| ViewModel | `GameViewModel`、`GameSimulation` | `IGameCommandSink`、`IGameSnapshotSource` | 直接绘制画面、读取键盘码 |
| View | `MainWindow`、`GameWidget` | `bind(...)`、Qt 窗口与画布 | 修改游戏内部状态、计算伤害 |
| App | `GameApplication` | `run()` | 游戏规则、具体绘制细节 |

## 2.3 数据流设计

项目中存在两条单向数据流。

第一条是输入通道：

```text
Qt key event
  -> GameWidget::keyPressEvent / keyReleaseEvent
  -> GameCommand
  -> IGameCommandSink::handle_command
  -> GameViewModel
  -> GameSimulation
```

View 将键盘事件翻译成 `InputAction` 和 `ButtonState`，例如 `MoveRight + Pressed` 或 `HeavyAttack + Triggered`。ViewModel 不需要知道用户按的是方向键、WASD、J 还是 Z，它只处理逻辑动作。

第二条是状态通道：

```text
GameSimulation
  -> GameSnapshot
  -> IGameSnapshotSource::snapshot
  -> GameWidget::updateSnapshot
  -> paintEvent
```

ViewModel 每帧生成完整的 `GameSnapshot`。View 只读该快照，并根据其中的 `phase`、`map`、`player`、`enemies`、`hud`、`result` 等字段绘制画面。

## 2.4 时间推进机制

游戏循环由 View 层的 `QTimer` 驱动，约每 16 ms 触发一次。触发时，`GameWidget` 根据 `QElapsedTimer` 计算真实的 delta time，并发出 `tickRequested(deltaSeconds, frameIndex)`。绑定器把 Tick 转成 `GameCommand::tick_command` 后交给 ViewModel。

这种设计兼顾了 Qt 应用的事件循环和 ViewModel 的独立性。定时器本身留在 View 层，规则推进留在 ViewModel 层；窗口只提供时间节奏，不直接参与移动、攻击或胜负判定。

## 2.5 状态快照边界

`GameSnapshot` 是 ViewModel 到 View 的唯一状态出口。它不是内部对象本身，而是某一帧的只读显示结果。快照中包含：

- `GamePhase`：标题、游玩、锁屏、暂停、失败、胜利等阶段。
- `MapSnapshot`：世界宽度、视口大小、镜头位置、街道边界、GO 提示。
- `ActorSnapshot`：玩家、敌人、Boss、特效的可绘制状态。
- `HudSnapshot`：玩家血量、精力、Boss 血条、连击和疲劳状态。
- `GameResultSnapshot`：胜负原因、用时和击败数量。

内部的攻击盒、碰撞矩形、敌人攻击冷却、遭遇战 ID、滚动锁状态等都保留在 ViewModel 内部，不向 View 暴露。这使得 View 可以保持稳定，即使后续增加更复杂的 AI 或连招系统，也只需要在快照层补充必要显示字段。
