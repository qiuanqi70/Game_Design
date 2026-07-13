# 5 ViewModel 层分报告

\sectionauthor{邱安琪}{3240105004}

## 5.1 层次定位

本工程遵循面向游戏类项目的 MVVM 实践规范。由于游戏场景中的核心数据（如实体状态、位置等）直接对应视图层的可绘制对象，无需进行二次业务模型转换，因此本架构合并了传统意义上的 Model 层，将其统一内聚于 ViewModel 层中。ViewModel 层负责把游戏规则封装为独立的模拟器（simulation）并将可绘制的状态快照提供给 View。View 只负责呈现和收集输入，所有玩法逻辑（移动、碰撞、伤害、敌人 AI、Boss 产生与判定等）都在 ViewModel/Simulation 中实现，且不依赖于 Qt 或绘图 API。

实现上该层由两部分组成：

- `GameViewModel`：对外门面，负责命令绑定、状态同步和事件触发（在代码中为 `GameViewModel`，位于 `code/viewmodel/GameViewModel.*`）。
- `GameSimulation`：内部仿真器，维护 `EntityState` 列表并推进物理/AI/战斗逻辑（位于 `code/viewmodel/GameSimulation.*` 和 `code/viewmodel/SimulationTypes.h`）。

这种将底层仿真逻辑（Simulation）直接内聚在 ViewModel 内部的设计，既简化了游戏数据的传递链路，避免了 Model 到 ViewModel 冗余的数据转换，又完美保持了核心逻辑对 UI 框架（Qt）的零依赖，极大地便利了无 UI 环境下的单元测试。

## 5.2 GameViewModel 门面

`GameViewModel` 负责把外部输入转成内部规则执行，并把仿真结果整理为 View 可直接使用的状态。

它的核心职责可以概括为三点：

1. 接收 View 发送的命令，如移动、攻击、跳跃、暂停、确认和重置；
2. 维护本地动作状态，包括按键状态、跳跃计时、攻击计时和玩家能量；
3. 将仿真结果同步到 `GameState`，并通过事件通知 View 更新。

在实现上，它主要依赖以下机制：

- `get_tick_command()`、`get_move_left_command()` 等命令方法：把外部输入封装为 `std::function`，形成典型的命令模式接口；
- `tick(dt, frameIndex)`：每帧更新动作计时器，并调用 `GameSimulation` 推进一步；
- `sync_state_from_simulation()`：把实体状态映射为 `GameState` 快照；
- `try_spend_energy()`：控制跳跃与攻击的能量消耗，防止连续高强度操作。

其中，当前工程采用的是一种轻量级的“命令回调”风格：`GameViewModel` 通过 `std::function` 暴露一组命令接口，如 `get_move_left_command()`、`get_primary_action_command()` 等，View 只需要注册这些回调即可。它并不是传统意义上定义独立 `Command` 类层次结构的完整命令模式实现，而是用函数对象把“请求”封装成可回调的接口，从而实现了命令分离与解耦。这样做的好处是，View 不需要知道内部规则流程，只需要发出“移动”“攻击”“暂停”等请求即可。事件通知则由 `fire(kGameStateChangedEvent)` 负责，在状态发生变化后，把更新通知给订阅者，从而实现 View 的刷新。

此外，时钟驱动也通过事件/回调机制来实现。`GameSimulation` 允许注册 tick 监听器，`GameViewModel` 在每帧收到时间流逝后调用 `step(dt)`，再将结果同步到状态快照。换言之，游戏逻辑的推进不是通过直接调用界面更新，而是通过“时间事件 + 状态回调”的方式完成。

## 5.3 GameSimulation 仿真核心

`GameSimulation` 是整个 ViewModel 层的规则引擎，负责实体状态、战斗逻辑和关卡推进。

它的主要职责包括：

- 维护玩家与敌人实体列表；
- 根据输入更新玩家位置；
- 处理敌人的移动与攻击行为；
- 处理攻击命中、伤害与死亡判定；
- 根据玩家推进情况生成 Boss，并更新胜负状态。

### 5.3.1 玩家与实体状态

仿真层使用 `EntityState` 维护每个实体。玩家为列表第一个实体，后续实体为敌人或 Boss。每个实体包含位置、血量、最大血量、是否存活等信息。

### 5.3.2 移动与战斗

- 玩家移动由 `player_move()` 负责，位置会被限制在世界范围内；
- 攻击由 `player_attack()` 处理，命中后会对敌人造成固定伤害，并在敌人死亡时更新 Boss 状态；
- 敌人 AI 由 `simulate_ai()` 处理，敌人会朝玩家方向靠近，并在接近时对玩家造成伤害。

### 5.3.3 Boss 与胜负判定

- 当玩家推进到指定触发位置后，`spawn_boss_if_needed()` 会生成 Boss；
- `update_boss_state()` 会持续检查 Boss 是否仍存活；
- 当玩家死亡时，视图层会看到 `GameOver`；当 Boss 被击败时，视图层会看到 `Win`。

## 5.4 状态快照与阶段切换

`GameViewModel` 的 `sync_state_from_simulation()` 会把仿真结果整理为对外的 `GameState`。

它主要输出以下内容：

- 玩家位置、血量、能量、朝向和动作状态；
- 敌人列表与 Boss 血条信息；
- 地图相机位置、推进比例和 HUD 状态；
- 当前阶段（Playing、Paused、GameOver、Win）。

其中阶段切换逻辑较为简单：

- 玩家死亡进入 `GameOver`；
- Boss 死亡进入 `Win`；
- 暂停时切换为 `Paused`；
- 确认键可在标题、失败或胜利状态下重开游戏。

## 5.5 小结

本工程的 ViewModel 层实现了一个简洁但完整的“输入—规则—状态”链路：

- `GameViewModel` 负责输入接收、状态同步与事件通知；
- `GameSimulation` 负责纯规则逻辑，包括移动、攻击、敌人 AI、Boss 生成与胜负判定。

这种结构使 View 只需关注显示与交互，而玩法逻辑则被集中封装在 ViewModel 层内，便于后续继续扩展更复杂的关卡与战斗系统。
