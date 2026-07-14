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

在 `code/viewmodel/GameViewModel.cpp 的`的 `GameViewModel::tick()` 中，具体实现是这样的：

```cpp
m_state.elapsedSeconds += dt;
update_action_timers(dt);

const float dx = (m_moveRight ? kMoveSpeed : 0.0f) - (m_moveLeft ? kMoveSpeed : 0.0f);
const float dy = (m_moveDown ? kMoveSpeed : 0.0f) - (m_moveUp ? kMoveSpeed : 0.0f);
if (dx != 0.0f || dy != 0.0f) {
    m_sim->player_move(dx * dt, dy * dt);
}

m_sim->step(dt);
sync_state_from_simulation();
notify_state_changed();
```

这段代码直接体现了 ViewModel 的职责链路：先更新局部状态，再推进仿真，再生成快照，最后通过 `fire(kGameStateChangedEvent)` 通知 View。与此同时，`sync_state_from_simulation()` 还会把玩家位置限制在 `0.0f ~ kWorldWidth` 和 `kStreetTop ~ kStreetBottom` 之间，确保内部数据不会越界。

## 5.4 GameSimulation 仿真核心

`GameSimulation` 是整个 ViewModel 层的规则引擎，负责实体状态、战斗逻辑和关卡推进。

### 5.4.1 实体与世界状态

仿真层使用 `EntityState` 维护每个实体。玩家为列表第一个实体，后续实体为敌人或 Boss。每个实体包含位置、血量、最大血量、是否存活等信息。这样的设计使得逻辑判断比较直观：所有规则都围绕一组实体进行，而不是围绕界面对象进行。

```cpp
struct EntityState {
    alleyfist::WorldPosition pos;
    EntityKind kind = EntityKind::Grunt;
    int hp = 0;
    int maxHp = 0;
    int id = 0;
    bool alive = true;
};

using EntityList = std::vector<EntityState>;
```

### 5.4.2 移动与战斗

在 `code/viewmodel/GameSimulation.cpp` 中，规则逻辑是直接写在几个关键函数里的：

- `player_move()` 会对玩家位置做边界限制：`p.pos.x = std::clamp(p.pos.x + dx, 0.0f, kWorldWidth);`，`p.pos.laneY = std::clamp(p.pos.laneY + dy, kStreetTop, kStreetBottom);`
- `simulate_ai()` 会根据敌人类型决定移动速度：`float speed = e.kind == EntityKind::Boss ? 55.0f : 30.0f;`，然后让敌人朝玩家方向靠近；
- `player_attack()` 会对附近敌人进行攻击判定，并执行 `e.hp -= 10;`，若敌人血量归零则设置 `e.alive = false;`。

普通小怪和 Boss 的战斗表现并不相同：小怪靠近玩家后对其造成较小伤害，而 Boss 的近距离伤害更强，且命中范围也更大。这些细节都直接体现在 `simulate_ai()` 和 `player_attack()` 的判断逻辑里。

### 5.4.3 Boss 与胜负判定

为了增强关卡感，仿真器还内置了 Boss 触发逻辑：

```cpp
constexpr float kBossTriggerX = 2350.0f;
constexpr float kBossSpawnOffsetX = 260.0f;
constexpr int kBossHealth = 140;
```

`spawn_boss_if_needed()` 会在玩家位置满足 `player.pos.x >= kBossTriggerX` 时生成 Boss，并把 `boss.pos.x` 设置为 `std::min(kWorldWidth - 80.0f, player.pos.x + kBossSpawnOffsetX)`；Boss 的初始血量为 `kBossHealth`。随后 `update_boss_state()` 会检查 `std::any_of(...)` 是否仍存在存活的 Boss，并把 `m_bossDefeated` 更新为 `true`。最后在 `code/viewmodel/GameViewModel.cpp` 的 `sync_state_from_simulation()` 中，若 `m_sim->boss_defeated()` 为真，就把阶段切换为 `GamePhase::Win`。

## 5.5 状态快照与阶段切换

`sync_state_from_simulation()` 是 ViewModel 向外输出状态的关键函数。它会把内部实体状态整理为 `GameState`，包括以下内容：

- 玩家位置、血量、能量、朝向和动作状态；
- 敌人列表与 Boss 血条信息；
- 地图相机位置、推进比例和 HUD 状态；
- 当前阶段（`Playing`、`Paused`、`GameOver`、`Win`）。

这一层的设计使 View 可以保持相对简单：它只需要读取一份快照，不需要自己去推断角色位置、敌人数量或阶段状态。对应地，ViewModel 也不需要知道屏幕上如何绘制，只需要保证快照字段语义明确即可。

在具体实现中，`sync_state_from_simulation()` 还会根据玩家位置计算 `progressRatio`，即 `player.x / 3000.0f` 的归一化值；同时根据玩家位置和视口尺寸计算 `cameraX`，公式为 `player.x - 960 * 0.42`，从而实现镜头的平滑跟随。这样，View 层只需直接读取 `GameState.map.cameraX` 与 `GameState.progressRatio`，就能得到场景推进和 HUD 进度的显示数据。

其中阶段切换逻辑为：

- 玩家死亡进入 `GameOver`；
- Boss 死亡进入 `Win`；
- 暂停时切换为 `Paused`；
- 确认键可在标题、失败或胜利状态下重开游戏。

## 5.6 小结

本工程的 ViewModel 层实现了一个简洁但完整的“输入—规则—状态”链路：

- `GameViewModel` 负责输入接收、状态同步与事件通知；
- `GameSimulation` 负责纯规则逻辑，包括移动、攻击、敌人 AI、Boss 生成与胜负判定。

这种结构使 View 只需关注显示与交互，而玩法逻辑则被集中封装在 ViewModel 层内，便于后续继续扩展更复杂的关卡与战斗系统。

## 5.7 技术难点的克服

开发 ViewModel 层过程中，主要遇到的技术难点主要有四个方面：

**难点一：输入与逻辑的解耦。** 由于 View 层传来的输入是按键事件，直接在逻辑层中处理会导致规则代码和界面事件耦合。解决方案是通过 `get_move_left_command()`、`get_primary_action_command()` 等 `std::function` 接口将输入包装为语义命令，再由 `GameViewModel` 中的 `make_move_command()` 和 `begin_attack()` 统一处理。

**难点二：Tick 与状态同步的一致性。** 如果每帧的逻辑推进和状态快照更新顺序不统一，就容易出现画面滞后或逻辑状态不同步。解决方案是将 `tick()` 设计为统一入口，先更新计时器与输入状态，再调用 `GameSimulation::step()`，最后统一通过 `sync_state_from_simulation()` 输出快照。

**难点三：动作系统与能量系统的协调。** 跳跃、攻击、能量恢复和冷却时间都需要同时控制，否则容易出现“连续高强度操作”或状态混乱。解决方案是通过 `m_jumpElapsed`、`m_attackTimer`、`m_playerEnergy` 与 `try_spend_energy()` 这组局部状态组合成一个相对清晰的状态管理机制。

**难点四：Boss 与胜负条件的判定。** 规则系统一开始较简单，但当 Boss 生成、敌人死亡和玩家胜负条件同时存在时，逻辑容易变得混乱。最终通过 `spawn_boss_if_needed()`、`update_boss_state()` 与 `boss_defeated()` 的分层处理，成功把 Boss 触发、存活检测和胜负切换拆解成明确的步骤。

## 5.8 团队协作情况

ViewModel 层作为项目四个模块之一，与 View、App 和 Common 层保持了持续沟通：

**协作亮点：**

- Common 层定义的 `GameState` 和通知机制在前期已经基本稳定，ViewModel 层据此独立完成逻辑与快照映射，减少了后期接口反复修改的成本。
- `GameViewModel` 暴露的命令接口与 View 层的 setter 接口命名基本对齐，联调时只需关注回调绑定，而不需要重写整个调用链。
- App 层在组合根中统一完成属性、命令和通知的注入，ViewModel 层只需实现对应接口即可，降低了模块间耦合。

## 5.9 阶段性成果展示

截至当前迭代，ViewModel 层已经完成了以下可演示的功能：

- 玩家移动与边界限制；
- 轻攻击、重攻击、跳跃与能量消耗控制；
- 小怪追击与近距离伤害逻辑；
- Boss 触发与 Boss 血量状态检测；
- 游戏阶段切换（`Playing`、`Paused`、`GameOver`、`Win`）；
- 将内部规则结果整理为可供 View 直接绘制的 `GameState` 快照。
