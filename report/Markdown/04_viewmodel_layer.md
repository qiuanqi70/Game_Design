# 5 ViewModel 层分报告

\sectionauthor{邱安琪}{3240105004}

## 5.1 层次定位

本工程遵循面向游戏类项目的 MVVM 实践规范。由于游戏场景中的核心数据（如实体状态、位置等）直接对应视图层的可绘制对象，无需进行二次业务模型转换，因此本架构合并了传统意义上的 Model 层，将其统一内聚于 ViewModel 层中。ViewModel 层负责把游戏规则封装为独立的模拟器（simulation）并将可绘制的状态快照提供给 View。View 只负责呈现和收集输入，所有玩法逻辑（移动、碰撞、伤害、敌人 AI、Boss 产生与判定等）都在 Simulation 中实现，且不依赖于 Qt 或绘图 API。

在实现上该层由两部分组成：

- `GameViewModel`：对外门面，负责命令绑定、状态同步和事件通知；
- `GameSimulation`：内部仿真器，维护 `EntityState` 列表并推进移动、攻击、敌人 AI 与 Boss 逻辑。

这种设计的优势在于：一方面避免了 Model 到 ViewModel 的冗余数据转换，另一方面使得核心逻辑完全不依赖 Qt 或绘图 API，因而可以在无界面环境下进行逻辑验证与后续扩展。

## 5.2 文件组织与编译

ViewModel 层位于 `code/viewmodel/` 目录下，主要由以下文件组成：

| 文件 | 作用 |
| --- | --- |
| `GameViewModel.h` / `GameViewModel.cpp` | 对外暴露命令接口、维护本地状态、负责与 View 同步 `GameState` |
| `GameSimulation.h` / `GameSimulation.cpp` | 实现规则引擎，处理实体移动、攻击、AI、Boss 生成与胜负判定 |
| `SimulationTypes.h` | 定义 `EntityState`、`EntityList`、实体种类和动作相关类型 |
| `CMakeLists.txt` | 编译为 `alleyfist_viewmodel` 静态库 |

编译配置如下：

```cmake
add_library(alleyfist_viewmodel STATIC
    GameViewModel.cpp
    GameSimulation.cpp
)

target_link_libraries(alleyfist_viewmodel
    PUBLIC
        alleyfist_common
)
```

与 View 层相比，ViewModel 层不直接链接 Qt 库，而是通过 Common 层提供的公共契约与 View 进行通信。这样可以保证逻辑层与界面层的解耦，后续无论替换为 SDL、OpenGL 还是其他框架，都不会影响游戏规则本身。

## 5.3 GameViewModel

`GameViewModel` 是整个 ViewModel 层对外暴露的核心入口。它不直接参与具体规则计算，而是负责把 View 的输入转成内部命令，并将规则结果整理为 View需要的状态快照。

### 5.3.1 核心职责

`GameViewModel` 的职责可以概括为三点：

1. 接收 View 发送的命令，例如移动、攻击、跳跃、暂停、确认和重置；
2. 维护本地动作状态，例如按键状态、跳跃计时、攻击计时和玩家能量；
3. 将 `GameSimulation` 的结果同步到 `GameState`，并通过事件通知 View 刷新界面。

### 5.3.2 命令注入与回调

当前工程采用的是轻量级的“命令回调”风格，而不是传统意义上的完整命令类层次结构。`GameViewModel` 通过 `std::function` 暴露命令接口，例如：

```cpp
std::function<void(float, std::uint64_t)> get_tick_command();
std::function<void(bool)> get_move_left_command();
std::function<void()> get_primary_action_command();
std::function<void()> get_pause_command();
```

View 层只需要注册这些回调即可，在收到输入后，ViewModel 再由内部方法处理具体逻辑。比如 `make_move_command()` 用来维护方向键状态，`begin_attack()` 则负责触发攻击并消耗能量。这种设计使 View 不必知道“攻击是如何发生的”，只需要发出“攻击”请求即可。

### 5.3.3 Tick 驱动与状态同步

`GameViewModel::tick()` 是整个逻辑循环的入口。每帧都会先更新跳跃、攻击和能量计时，然后根据当前按键状态调用 `GameSimulation::player_move()` 与 `GameSimulation::step()`，最后通过 `sync_state_from_simulation()` 将内部实体状态映射为对外的 `GameState`。这一过程体现了 ViewModel 的典型职责链路：

```text
输入命令 → 本地状态更新 → Simulation 推进 → 状态快照 → 事件通知
```

在实现上，`fire(kGameStateChangedEvent)` 用于在状态变化后通知订阅者，View 层收到通知后即可调用重绘，从而形成完整的“变化通知—数据读取—界面刷新”闭环。

## 5.4 GameSimulation 仿真核心

`GameSimulation` 是整个 ViewModel 层的规则引擎，负责实体状态、战斗逻辑和关卡推进。

### 5.4.1 实体与世界状态

仿真层使用 `EntityState` 维护每个实体。玩家为列表第一个实体，后续实体为敌人或 Boss。每个实体包含位置、血量、最大血量、是否存活等信息。这样的设计使得逻辑判断比较直观：所有规则都围绕一组实体进行，而不是围绕界面对象进行。

### 5.4.2 移动与战斗

`GameSimulation` 的主要规则方法包括：

- `player_move()`：根据输入更新玩家的位置，并限制在世界范围内；
- `player_attack()`：对附近敌人造成固定伤害，并在部分敌人死亡时更新 Boss 状态；
- `simulate_ai()`：让敌人朝玩家方向接近，并在接近时对玩家造成伤害。

其中，玩家的移动和攻击是在逻辑层完成的，而不是由 View 直接处理，因此 View 层可以保持“只负责展示”这一职责边界。

### 5.4.3 Boss 与胜负判定

为了增强关卡感，仿真器还内置了 Boss 触发逻辑：

- `spawn_boss_if_needed()`：当玩家推进到指定位置后生成 Boss；
- `update_boss_state()`：持续检查 Boss 是否仍然存活；
- `boss_defeated()`：在 Boss 被击败后向外部报告胜负状态。

最终，`GameViewModel` 会根据这些结果切换当前阶段为 `Playing`、`Paused`、`GameOver` 或 `Win`，形成完整的战斗闭环。

## 5.5 状态快照与阶段切换

`sync_state_from_simulation()` 是 ViewModel 向外输出状态的关键函数。它会把内部实体状态整理为 `GameState`，包括以下内容：

- 玩家位置、血量、能量、朝向和动作状态；
- 敌人列表与 Boss 血条信息；
- 地图相机位置、推进比例和 HUD 状态；
- 当前阶段（`Playing`、`Paused`、`GameOver`、`Win`）。

这一层的设计使 View 可以保持相对简单：它只需要读取一份快照，不需要自己去推断角色位置、敌人数量或阶段状态。对应地，ViewModel 也不需要知道屏幕上如何绘制，只需要保证快照字段语义明确即可。

其中阶段切换逻辑较为清晰：

- 玩家死亡进入 `GameOver`；
- Boss 死亡进入 `Win`；
- 暂停时切换为 `Paused`；
- 确认键可在标题、失败或胜利状态下重开游戏。

## 5.6 小结

本工程的 ViewModel 层实现了一个简洁但完整的”输入—规则—状态”链路：

- `GameViewModel` 负责输入接收、状态同步与事件通知；
- `GameSimulation` 负责纯规则逻辑，包括移动、攻击、敌人 AI、Boss 生成与胜负判定。

这种结构使 View 只需关注显示与交互，而玩法逻辑则被集中封装在 ViewModel 层内，便于后续继续扩展更复杂的关卡与战斗系统。

## 5.7 技术难点的克服

开发 ViewModel 层过程中，主要遇到以下技术难点：

**难点一：输入与逻辑的解耦。** 最初版本在 Common 层定义了 `InputAction` 枚举和 `GameCommand` 结构体，View 发出 `GameCommand`，ViewModel 通过 `process_input_command()` 做 switch-case 分发。这种设计的耦合问题在于：Common 层被迫包含输入设备相关的概念（`MoveLeft`、`LightAttack` 等），而 ViewModel 也需要理解按下/释放的状态语义。在袁老师指导下，我们将命令改为 `std::function` 回调注入——`get_move_left_command()` 返回 `std::function<void(bool)>`，`get_primary_action_command()` 返回 `std::function<void()>`。ViewModel 不再需要知道”按键”这个概念，只需要响应”左移动开启/关闭”这样的语义信号。

**难点二：Tick 与状态同步的一致性。** 每帧的逻辑推进涉及多个步骤：更新动作计时器、处理移动输入、调用 `GameSimulation::step()`、同步内部实体到 `GameState` 快照、触发通知。如果顺序不统一，容易出现画面滞后或逻辑状态不同步。解决方案是将 `tick()` 设计为统一入口，严格按”计时器更新 → 输入处理 → 模拟推进 → 状态同步 → 通知触发”的顺序执行，保证每帧的状态一致性。

**难点三：动作系统与能量系统的协调。** 跳跃、攻击、能量恢复和疲劳状态需要同时管理——玩家不能在被攻击硬直时跳跃，不能在空中连续跳跃，能量耗尽后需要恢复期。解决方案是通过 `m_jumpElapsed`、`m_attackTimer`、`m_playerEnergy`、`m_exhaustedWarningTimer` 以及 `try_spend_energy()` 组成一组局部状态管理，`is_gameplay_active()` 和 `is_player_action_locked()` 作为守卫条件，在命令入口处拦截不合法的操作。

**难点四：Boss 与胜负条件的判定。** 当 Boss 生成、敌人死亡和玩家死亡同时可能发生时，阶段切换逻辑容易混乱。最终通过 `spawn_boss_if_needed()`、`update_boss_state()` 与 `boss_defeated()` 的分层处理，把 Boss 触发、存活检测和胜负切换拆解为独立的步骤，再由 `sync_state_from_simulation()` 统一设置 `GameState.phase`。

## 5.8 团队协作情况

ViewModel 层作为项目核心规则层，与 View、App 和 Common 层保持了持续沟通。

**协作亮点：**

- Common 层定义的 `GameState` 和 `EventTrigger` 在前期经过几轮迭代后趋于稳定，ViewModel 层据此独立完成逻辑与快照映射，减少了接口反复修改的成本。
- `GameViewModel` 暴露的命令接口（如 `get_move_left_command`、`get_primary_action_command`）与 View 层的 setter 命名一一对应，联调时几乎不需要额外沟通。
- App 层在组合根中统一完成 properties / commands / notification 的注入，ViewModel 只负责实现接口，不参与绑定细节。

**可改进之处：**

- 仿真层（`GameSimulation`）和 ViewModel 层的职责边界可以更清晰——当前部分状态（如 `m_moveLeft` 等移动标志）在 ViewModel 维护，部分在 Simulation 维护，后续可以统一为 Simulation 内部持有所有规则状态。
- 缺少 ViewModel 层的独立单元测试。虽然逻辑层不依赖 Qt，便于测试，但目前 `GameSimulationTest.cpp` 的覆盖范围有限。

## 5.9 阶段性成果展示

截至当前迭代，ViewModel 层已经完成了以下可演示的功能：

- 玩家移动与边界限制（八方向按键 → 世界坐标位移 + 街道边界钳制）
- 轻攻击、重攻击、跳跃，每个动作带独立的能量消耗和冷却计时
- 精力恢复系统，精力耗尽后触发疲劳状态
- 小怪 AI 追击与近距离伤害
- Boss 触发（玩家推进到指定位置后自动生成）与 Boss 血量状态检测
- 游戏阶段切换（Playing → Paused ↔ Playing / Playing → GameOver / Playing → Win）
- 将内部规则结果整理为可供 View 直接绘制的 `GameState` 快照

## 5.10 总体心得与个人感悟

作为 ViewModel 层开发者，本次项目的最大收获是对 MVVM 中”规则与表现的分离”有了实践层面的理解。之前写游戏时习惯于在键盘事件里直接改坐标、在绘制函数里顺手写碰撞判断，开发速度快但后续改动成本很高。这次严格按照 MVVM 设计后，虽然前期投入更多时间拆分层级和定义接口，但后期联调和迭代确实更顺畅——改 AI 行为不需要动 View 的绘制代码，改界面布局也不需要担心影响游戏规则。

另一个体会是：ViewModel 层的”门面”设计很重要。`GameViewModel` 对外暴露的是属性（`get_game_state()`）和命令（`get_xxx_command()`），内部委托 `GameSimulation` 处理规则细节，这种两层划分使得代码结构清晰，也便于后续对仿真核心进行单元测试。

智能体的使用：在 ViewModel 层开发中，使用了 Claude Code 辅助生成命令回调的样板代码和 `sync_state_from_simulation()` 的初始框架，节省了部分机械编码时间。但智能体在处理动作状态机等复杂逻辑时容易漏掉边界条件（如”跳跃中不能攻击”需要在两个不同的代码路径中添加守卫），最终的逻辑细节仍需要人工审核和修正。
