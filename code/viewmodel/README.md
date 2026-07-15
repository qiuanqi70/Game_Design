# ViewModel 迭代设计方案

本文档用于指导后续在 [code/viewmodel](code/viewmodel) 下推进 P0/P1 迭代。当前目标不是先把所有功能一次性做完，而是先把“公共状态契约已经明确”的内容，完整地映射到 ViewModel 的内部仿真与 GameState 快照输出。

## 1. 设计目标

基于当前 Common 层已经补充的状态结构，ViewModel 需要完成以下能力：

- 让 Boss 遭遇、波次、Intro/Fighting/Cleared 阶段可被 View 直接消费。
- 让命中、受击、击退、硬直等反馈以 `impactRevision` 和 `lastImpact` 的形式暴露给 View。
- 用独立的投射物与补给道具状态表达远程攻击和掉落道具。
- 为未来扩展四类敌人、敌人波次、Boss 战和道具系统预留清晰结构。

---

## 2. 现状与缺口

当前实现中，ViewModel 主要存在三点不足：

1. 只存在一个简化的 `EntityKind::Grunt` / `EntityKind::Boss`，没有区分巡逻、伏击、冲撞、远程敌人。
2. `GameState` 里缺少对 Boss 遭遇、敌人波次、投射物、补给道具和命中反馈的完整同步。
3. 现有 `GameSimulation` 逻辑偏“单体敌人近战攻击”，不足以支撑 P0 里的 Boss 锁屏、受击反馈和未来的多波次设计。

因此，下一阶段的重点不是“重写 UI”，而是把 ViewModel 从“单一简化模拟器”升级为“可表达游戏事件和表现状态的状态机”。

---

## 3. 推荐的总体结构

### 3.1 责任划分

- [code/viewmodel/GameSimulation.h](code/viewmodel/GameSimulation.h) / [code/viewmodel/GameSimulation.cpp](code/viewmodel/GameSimulation.cpp)
  - 负责内部规则、AI、碰撞、敌人生成、波次推进、投射物和补给道具生命周期。
  - 只维护 ViewModel 内部数据，不直接面向 View。

- [code/viewmodel/GameViewModel.h](code/viewmodel/GameViewModel.h) / [code/viewmodel/GameViewModel.cpp](code/viewmodel/GameViewModel.cpp)
  - 负责把内部仿真状态映射为 [code/common/game_state.h](code/common/game_state.h) 中的 `GameState`。
  - 负责接收输入命令、驱动 tick、更新玩家动作状态和通知 View 更新。

- [code/viewmodel/SimulationTypes.h](code/viewmodel/SimulationTypes.h)
  - 定义 ViewModel 内部使用的扩展类型，例如敌人动作、攻击事件、投射物和道具数据结构。

### 3.2 设计原则

- ViewModel 仍然是 `GameState` 的唯一写入方。
- Common 层只保存“已经计算好、View 可直接读取”的结果。
- 不把 AI、数值公式、碰撞半径、动画参数、Boss 出场样式等写进 Common。
- 保留 `ActorState::id` 作为稳定身份标识，避免依赖数组下标。

---

## 4. 具体修改方案

### 4.1 扩展内部实体与状态类型

先在 [code/viewmodel/SimulationTypes.h](code/viewmodel/SimulationTypes.h) 中给内部实体增加更细的类型和状态字段：

- `EntityKind` 扩展为：
  - `Player`
  - `Patroller`
  - `Ambusher`
  - `Charger`
  - `Ranged`
  - `Boss`

- 为每个敌人加入内部字段：
  - `enemyType`：敌人类型
  - `behaviorState`：如 `Idle`、`Patrol`、`Ambush`、`Charge`、`RangedAttack`、`Hurt`
  - `attackCooldown`：攻击冷却时间
  - `chargeTimer`：冲撞持续时间
  - `ambushCooldown`：伏击触发冷却
  - `patrolRangeLeft/Right`：巡逻边界
  - `spawnWaveId`：所属波次 ID

- 增加内部结构：
  - `CombatHitEvent`：记录某个角色被命中的目标、伤害、命中等级。
  - `ProjectileStateVm`：内部投射物数据，包含发射者、速度、生命周期、命中标记等。
  - `PickupStateVm`：内部补给道具数据，包含掉落来源、剩余时间等。

这样做的目的是让 ViewModel 可以在内部“先生成事件”，再统一映射到 Common 的 `ActorState`、`ProjectileState` 和 `PickupState`。

### 4.2 把仿真逻辑从“近战小怪”升级为“带状态机的敌人系统”

在 [code/viewmodel/GameSimulation.cpp](code/viewmodel/GameSimulation.cpp) 中，建议把原来的 `simulate_ai()` 拆成以下几个步骤：

1. `update_encounter_state(dt)`
   - 根据玩家进度或触发条件推进 Encounter 阶段。
   - 当进入 `Intro` 时，设置 `EncounterState::phase = Intro`。
   - 当真正进入战斗时，切换到 `Fighting`。
   - 当 Boss 被击败或波次被清空时，切换到 `Cleared`。

2. `update_enemy_behaviors(dt)`
   - 对每类敌人执行不同逻辑：
     - `Patroller`：在边界内来回移动。
     - `Ambusher`：玩家靠近后进入 `Ambush` 状态，短暂后攻击。
     - `Charger`：靠近时触发 `Charge`，短时加速冲撞。
     - `Ranged`：保持距离攻击，发射投射物。
     - `Boss`：进入 Boss 战逻辑，按阶段切换动作。

3. `update_projectiles(dt)`
   - 更新投射物位置、重力和命中判定。
   - 命中后标记为已命中，并生成一次命中事件。

4. `update_pickups(dt)`
   - 处理道具存在时间、被拾取和消失逻辑。

5. `collect_combat_events()`
   - 汇总所有命中、受击、击退、硬直状态，统一写入视图模型层的表现状态。

### 4.3 为 Boss 遭遇和波次设计统一状态

建议在 [code/viewmodel/GameViewModel.cpp](code/viewmodel/GameViewModel.cpp) 中把当前的 Boss 触发逻辑抽象为统一的 `EncounterState`：

- `EncounterKind::None`：无遭遇。
- `EncounterKind::EnemyWave`：普通敌人波次。
- `EncounterKind::Boss`：Boss 战。

- `EncounterPhase::Intro`：显示 Boss 出场或波次提示。
- `EncounterPhase::Fighting`：正式战斗中。
- `EncounterPhase::Cleared`：清场完成。

输出到 `GameState::encounter` 的字段包括：

- `kind`
- `phase`
- `currentWave`
- `totalWaves`
- `remainingEnemies`
- `introProgress`

这一步的重点是让 View 能直接看到“当前到底是Boss战还是普通波次”，而不是通过猜测数组内容来判断。

### 4.4 让命中反馈沉淀为公共状态

这是 P0 中最值得优先落地的部分。建议在 ViewModel 内部维护一个“本帧命中事件列表”，然后统一更新角色的状态：

- 当角色被命中时：
  - `impactRevision += 1`
  - `lastImpact = ImpactLevel::Light/Heavy`
  - 记录该角色的受击方向或简化反馈类型

- 当角色发动攻击命中目标时：
  - 目标的 `impactRevision` 增加
  - `lastImpact` 设置为对应等级

该逻辑应在 [code/viewmodel/GameSimulation.cpp](code/viewmodel/GameSimulation.cpp) 中完成，随后由 [code/viewmodel/GameViewModel.cpp](code/viewmodel/GameViewModel.cpp) 转成 `GameState::enemies` 中对应角色的状态。

这样 View 就能用以下逻辑做效果：

- `impactRevision` 变化 → 播放命中粒子或闪光
- `lastImpact` 决定粒子强度和屏幕震动强度

### 4.5 增加投射物与补给道具的同步输出

当前 [code/common/game_state.h](code/common/game_state.h) 已经提供了 `ProjectileState` 和 `PickupState`，因此 ViewModel 需要把内部模拟结果映射进去：

- `GameState::projectiles`
  - 保存当前仍然有效的投射物
  - 由 ViewModel 中的内部投射物列表生成

- `GameState::pickups`
  - 保存当前场景中可拾取的道具
  - 由随机掉落或固定掉落规则生成

建议在 [code/viewmodel/GameSimulation.h](code/viewmodel/GameSimulation.h) 中增加内部字段：

- `std::vector<ProjectileStateVm> m_projectiles`
- `std::vector<PickupStateVm> m_pickups`

在 [code/viewmodel/GameViewModel.cpp](code/viewmodel/GameViewModel.cpp) 的 `sync_state_from_simulation()` 中，将它们转换为公共状态数组。

### 4.6 把 HUD 状态变成“可表达 Boss 战与进度”的结构

当前 [code/common/game_state.h](code/common/game_state.h) 已经有 `HudState`，建议在 ViewModel 中把它扩展为：

- `playerHealth`：由玩家当前 HP 生成
- `playerEnergy`：由当前能量值生成
- `bossHealth`：Boss 存活时显示；Boss 消失则清空
- `showBossHealth`：Boss 战存在时为 `true`
- `playerExhausted`：能量耗尽时为 `true`

同时，当前 `GameState::progressRatio` 继续由玩家 X 轴位置映射，保持和当前关卡进度条的使用方式一致。

---

## 5. 推荐的落地顺序

### 第一阶段（P0，优先级最高）

1. 把内部实体类型扩成 5 类敌人，并让 `GameSimulation` 能区分敌人行为。
2. 增加遇到 Boss 的统一 `EncounterState`，实现 `Intro -> Fighting -> Cleared` 的简化流程。
3. 为命中/受击/硬直增加 `impactRevision` 和 `lastImpact` 的更新逻辑。
4. 把 Boss 血条、进度条和当前遭遇信息写到 `GameState::hud` / `GameState::encounter`。

### 第二阶段（P1，后续扩展）

5. 增加远程投射物与补给道具的内部逻辑。
6. 增加波次生成与普通敌人波次的触发器。
7. 为标题页和结算页预留更清晰的阶段状态切换。

---

## 6. 具体实现文件清单

建议按以下文件逐步改造：

- [code/viewmodel/SimulationTypes.h](code/viewmodel/SimulationTypes.h)
  - 扩展内部状态结构。

- [code/viewmodel/GameSimulation.h](code/viewmodel/GameSimulation.h)
  - 增加敌人、投射物、补给道具、遭遇状态字段。

- [code/viewmodel/GameSimulation.cpp](code/viewmodel/GameSimulation.cpp)
  - 实现敌人行为、Boss 遭遇、命中反馈、投射物和道具逻辑。

- [code/viewmodel/GameViewModel.h](code/viewmodel/GameViewModel.h)
  - 增加必要的内部状态管理字段。

- [code/viewmodel/GameViewModel.cpp](code/viewmodel/GameViewModel.cpp)
  - 把内部仿真状态映射到 `GameState`。

---

## 7. 审核时重点关注的问题

你在审核时，建议重点看下面 4 个点：

1. `GameState` 是否已经能完整表达当前遭遇和 Boss 状态，而不是依赖 UI 自己猜测。
2. `impactRevision` / `lastImpact` 是否已经能稳定支持后续粒子和震动效果。
3. 新敌人行为是否足够“可扩展”，后续新增敌人时能沿用同一套状态机。
4. `GameSimulation` 是否仍保持“内部规则”，而没有把 View 相关表现逻辑泄漏进来。

如果你认可这套方案，我下一步就开始按这个设计把代码真正落地。