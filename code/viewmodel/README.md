# ViewModel 层设计与规则

本目录实现游戏的规则系统和 MVVM 中的 ViewModel。它不依赖 Qt，也不读取键盘码或执行绘制。

## 1. 文件职责

| 文件 | 职责 |
| --- | --- |
| `SimulationTypes.h` | 定义仅供规则层使用的实体、玩家动作、敌人行为、投射物和掉落物状态 |
| `GameSimulation.h/.cpp` | 保存唯一的玩法状态，处理输入、动作、碰撞、AI、波次、Boss、投射物和掉落物 |
| `GameViewModel.h/.cpp` | 接收语义命令，驱动 Simulation，将规则状态映射为 `GameState`，并发送变化通知 |

View 只能通过 `GameViewModel::get_game_state()` 读取公共快照，通过 `get_*_command()` 发送语义命令。View 不访问 `EntityState`，也不参与伤害、能量或阶段计算。

## 2. 状态所有权

一个状态只能有一个写入方。

`GameSimulation` 独占以下规则状态：

- 玩家位置、朝向、移动按键状态；
- 跳跃高度、攻击种类、动作计时和命中帧；
- 玩家能量、恢复和能量不足提示计时；
- 实体生命、受击、敌人 AI 和死亡展示计时；
- 投射物、掉落物及其生命周期；
- 普通波次、移动门禁、Boss Intro、Boss 战和胜利就绪状态；
- 游戏规则经过的时间。

`GameViewModel` 独占以下应用状态：

- `GameState` 公共快照；
- `Title`、`Playing`、`Paused`、`GameOver`、`Win` 页面阶段；
- ViewModel 到 View 的 notification。

能量、跳跃和攻击不在两层重复保存。Simulation 只保存内部 `ProjectileVm` / `PickupVm`，ViewModel 每次同步时直接映射为 Common 的 `ProjectileState` / `PickupState`，不存在第二份公共镜像。

## 3. 命令与数据流

```text
View input
  -> GameViewModel command
  -> GameSimulation input/action request
  -> GameSimulation::step(dt)
  -> GameViewModel::sync_state_from_simulation()
  -> GameState
  -> kGameStateChangedEvent
  -> View repaint
```

公开命令保持老师示例采用的回调形式：

```cpp
std::function<void(float, std::uint64_t)> get_tick_command();
std::function<void(bool)> get_move_left_command();
std::function<void()> get_primary_action_command();
std::function<void()> get_pause_command();
```

`frameIndex` 为跨层兼容参数，当前规则不依赖它。移动命令的 `bool` 表示按下或释放。按下只在 `Playing` 阶段接受；释放始终转发，避免方向状态卡住。进入暂停、失败或胜利状态时会清空移动输入。

Simulation 构造后即可 `step()`，没有额外的 `start()` / `stop()` 生命周期。暂停由 ViewModel 停止推进 tick 实现。`reset()` 会重建规则状态，因为内部容器可能分配内存，所以它不声明 `noexcept`。

## 4. 每帧推进顺序

`GameSimulation::step(dt)` 只接受有限且大于零的 `dt`。无效、负数、零、NaN 和无穷值不会改变规则状态。

有效帧按以下顺序推进：

1. 增加规则时间；
2. 更新能量、疲劳提示、受击、跳跃和玩家攻击计时；
3. 当攻击跨过命中帧时结算一次命中；
4. 根据四向输入移动玩家，并应用当前波次或 Boss 场地边界；
5. 推进普通波次和 Encounter；
6. 在普通波次全部完成后推进 Boss Intro 并生成 Boss；
7. 更新各类敌人 AI 和死亡掉落；
8. 更新死亡展示、投射物和掉落物；
9. 更新 Boss 死亡与胜利就绪状态。

顺序是规则的一部分。例如玩家攻击在敌人 AI 前到达命中帧时，敌人会先受击；掉落物在死亡处理后才可能被拾取。

## 5. 玩家规则

### 5.1 移动与跳跃

- 移动速度：`220 unit/s`；
- 街道纵深范围：`300 <= laneY <= 500`；
- 跳跃持续时间：`0.55 s`；
- 跳跃最高点：`90`；
- 跳跃消耗：`18` 能量；
- 跳跃轨迹：`z = height * sin(pi * progress)`。

跳跃高度保存在 Simulation 的玩家位置中，并参与近战和投射物碰撞。它不是只用于绘制的动画值。空中发起攻击时输出行为为 `AirAttack`；攻击锁定期间不能再次攻击，也不能从地面开始新的跳跃。

### 5.2 轻重攻击

| 动作 | 持续时间 | 命中帧 | 能量 | 伤害 | Impact | 基础范围 X/Y |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| 轻攻击 | `0.18 s` | `0.08 s` | `12` | `10` | `Light` | `55 / 35` |
| 重攻击 | `0.30 s` | `0.18 s` | `25` | `18` | `Heavy` | `72 / 44` |

动作请求只启动攻击，不立即扣除敌人生命。Simulation 在计时跨过命中帧时进行一次碰撞和伤害结算，随后用动作锁阻止重复输入绕过攻击间隔。Boss 因体型更大，命中范围有额外补偿。

玩家受击期间不能攻击或跳跃。受击会中断尚未命中的玩家攻击和当前跳跃，并把玩家高度复位到地面。玩家受击无敌时间由 `hurtTimer` 控制，避免同一时间被多个接触攻击重复扣血。

### 5.3 能量

- 最大能量：`100`；
- 自然恢复：`28/s`；
- 能量不足提示：`0.65 s`；
- Energy 掉落恢复：`20`。

消耗和恢复都只修改 Simulation 的 `m_playerEnergy`。ViewModel 仅将当前值映射到 HUD。

## 6. 敌人 AI

`simulate_ai()` 只负责所有敌人的公共前置处理和分发。每种敌人的规则位于独立函数中：

- `update_patroller()`：在巡逻边界内移动，接近后按命中帧近战；
- `update_ambusher()`：进入范围后蓄势并向玩家突进；
- `update_charger()`：经过 windup 后高速冲撞；
- `update_ranged_enemy()`：进入射程后蓄势并生成投射物；
- `update_boss()`：追踪玩家并执行伤害更高的冲撞。

公共逻辑统一处理受击恢复、攻击冷却、朝向和死亡掉落。新增敌人时应增加明确的 `EntityKind`、生成配置、独立更新函数和 ViewModel 的 `ActorKind` 映射，不能继续向某个无关敌人的分支叠加特殊条件。

## 7. 投射物与掉落物

远程投射物分别保存三个速度分量：

- `velocityX`：世界横向速度；
- `velocityLaneY`：街道纵深速度；
- `velocityZ`：高度速度。

生成投射物时，Simulation 根据与目标的横向距离计算预计飞行时间，再分别求出 X、laneY 的速度和能够落到目标高度的初始 `velocityZ`。每帧只对 `velocityZ` 应用重力：

```text
x     += velocityX * dt
laneY += velocityLaneY * dt
z     += velocityZ * dt
velocityZ -= gravity * dt
```

因此地面弹道可以命中站立玩家，而玩家在弹道发射后跳到足够高度可以规避。laneY 和 z 不共享速度，避免纵深移动被错误当成重力。

普通敌人死亡后掉落 Health，Ranged 掉落 Energy，Boss 同时掉落两种。掉落物有固定生命周期；玩家进入拾取范围后，Simulation 立即应用恢复并移除掉落物。

## 8. 波次、门禁与 Boss

关卡包含三个普通波次。每波存活敌人未清空前，玩家的最远 X 位置分别受限于：

| 波次 | 右侧门禁 |
| --- | ---: |
| 1 | `760` |
| 2 | `1540` |
| 3 | `2260` |

清空一波后进入 `1.0 s` 的 Transition，再生成下一波并放开新的区域。第三波完成后允许玩家移动到 `2350`，进入 `2.8 s` 的 Boss Intro。Intro 完成后生成 Boss，并把玩家限制在 `2180..2920` 的 Boss 场地。

Boss 死亡时 Encounter 立即进入 `Cleared`。死亡展示计时归零后 `boss_victory_ready()` 才变为 true，ViewModel 随后把公共阶段切换为 `Win`。因此 Boss 的死亡帧仍可展示，不会在扣完生命的同一帧直接消失。

## 9. GameState 映射

`sync_state_from_simulation()` 每次从 Simulation 生成一份可绘制快照：

- 玩家与敌人的公共 `ActorState`；
- 当前可见的死亡展示；
- 投射物和掉落物；
- 玩家、Boss 血条和能量提示；
- Encounter、关卡进度和相机位置；
- Game Over 与 Win 结果。

内部 `EntityState` 不通过 `GameViewModel` 暴露。View 只能持有稳定的 `const GameState*`。Snapshot 映射可以包含绘制所需的尺寸、相机和掉落物浮动效果，但不能反向修改 Simulation。

## 10. 不变量与扩展要求

维护或扩展规则时必须保持以下不变量：

1. `entities().front()` 始终是玩家，实体 ID 在一次游戏中唯一；
2. `alive == false` 的实体不能再移动、攻击或受伤；
3. 一次攻击最多结算一次命中；
4. 玩家规则状态只由 Simulation 写入；
5. 无效 `dt` 不改变任何规则状态；
6. 普通波次未完成时不能触发 Boss；
7. ViewModel 不提供可变实体入口，不允许调用方绕过规则修改生命或计时器；
8. 内部 Vm 类型只映射一次到 Common，不能重新引入同步镜像。

测试必须通过公开命令和只读状态到达场景。禁止使用 `const_cast`、friend 或测试专用生产接口伪造 `hp == 0 && alive == true` 等不可达状态。复杂流程应使用有最大步数的确定性推进 helper，失败时明确报告未达到的业务状态。
