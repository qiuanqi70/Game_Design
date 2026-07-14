# Common 层接口说明

`common/` 保存 View 与 ViewModel 都需要理解的公共契约，包括基础值类型、轻量通知工具，以及本游戏用于属性绑定的只读状态对象。它不放 Qt 类型，不放具体按键动作，也不放游戏规则命令。

## 当前文件

| 文件 | 职责 |
| --- | --- |
| `types.h` | 跨层共享的基础值类型，例如尺寸、世界坐标和资源条。 |
| `notification.h` | `std::function` 形式的事件通知工具。 |
| `game_state.h` | 本游戏的公共状态接口，例如 `GameState`、`ActorState` 和 `MapState`。 |
| `common.h` | 便捷入口，统一包含上述 common 头文件。 |

## 命令模式

命令不再放在 common 层。具体输入动作、tick 命令和玩法命令应由负责 ViewModel 的代码定义。

命令绑定建议参考老师 Plane 示例，使用 `std::function`：

```cpp
std::function<void(int)> get_move_command();
```

View 持有这个函数对象，在 UI 事件发生时调用它；ViewModel 返回捕获自身的 lambda，在 lambda 里执行对应业务逻辑。

## 事件通知

事件通知使用 `std::function`，而不是 Qt 信号槽或函数指针加上下文指针。

`notification.h` 提供：

```cpp
using EventNotification = std::function<void(std::uint32_t eventId)>;
```

触发者通过 `add_notification()` 保存回调，事件发生时调用 `fire(eventId)`。接收者可以传入普通函数、lambda 或绑定成员函数的函数对象。

示例：

```cpp
trigger.add_notification([this](std::uint32_t eventId) {
    handle_event(eventId);
});
```

`eventId` 的具体含义由触发者所在层定义，common 层不定义具体属性 id 或玩法事件 id。

## 属性绑定

老师示例里没有一个通用 `snapshot.h`。Plane 里 View 绑定的是 `const AirMap*`，Book 里 View 绑定的是 `serial/name/summary/price` 指针，Meitu 里 View 绑定的是图片指针。属性变化时只通过通知 id 告诉 View 哪个属性变了。

- `types.h` 放最基础的值类型，例如 `Size`、`WorldPosition`、`ResourceBar`。
- `game_state.h` 放本游戏当前需要跨层共享的具体属性容器，例如 `GameState`、`ActorState`、`HudState`。
- 这些类型不带 `View` 前缀，因为它们属于 Common 公共状态契约，不是 View 层私有结构。

## 下一轮迭代公共状态

下一轮迭代计划加入四类敌人、敌人波次、受击反馈、Boss 出场、远程投射物和补给道具。Common 只描述 ViewModel 已经计算完成、View 绘制时必须读取的结果，不保存 AI、碰撞、数值和动画实现。

### 角色类型

`ActorKind` 区分 View 需要采用不同外观绘制的角色：

- `Player`：玩家角色；
- `Patroller`：在指定区域内来回移动的巡逻型敌人；
- `Ambusher`：玩家靠近后从隐藏位置冲出的伏击型敌人；
- `Charger`：攻击时短时间加速的冲撞型敌人；
- `Ranged`：通过投掷物进行攻击的远程型敌人；
- `Boss`：关卡首领。

巡逻边界、伏击触发距离、冲撞速度、远程攻击距离和所有敌人数值都属于 ViewModel 内部规则，不加入 `ActorState`。

### 角色动作与受击状态

`ActorActionState` 新增以下跨层动作：

- `Ambush`：伏击型敌人正在从隐藏位置冲出；
- `Charge`：冲撞型敌人正在高速冲撞；
- `RangedAttack`：远程型敌人正在进行投掷动作；
- `Hurt`：角色正在表现受击或硬直。

ViewModel 决定动作何时开始和结束，View 只选择对应姿态、残影、速度线或受击闪烁。蓄力时间、硬直时间、击退速度、攻击范围和无敌时间不进入 Common。角色移动和击退后的结果继续通过 `ActorState::position` 传递。

### 稳定角色标识与命中反馈

`ActorState::id` 来自 ViewModel 内部实体 ID，用于让 View 在敌人列表发生增删后仍能识别同一个角色。View 不应使用 `enemies` 数组下标作为角色身份。

`impactRevision` 表示角色累计发生了多少次需要表现的有效命中。每确认一次新命中，ViewModel 将修订号递增，并更新 `lastImpact`：

```cpp
target.impactRevision++;
target.lastImpact = ImpactLevel::Heavy;
```

View 按角色 ID 保存上一次已经处理的修订号。修订号变化时，View 可以在角色当前位置创建一次粒子、命中闪光或屏幕震动：

```cpp
if (actor.impactRevision != lastRevisionByActor[actor.id]) {
    play_impact_feedback(actor.position, actor.lastImpact);
    lastRevisionByActor[actor.id] = actor.impactRevision;
}
```

`ImpactLevel` 只表达 `None`、`Light` 和 `Heavy` 三种跨层冲击等级。粒子数量、颜色、生命周期和屏幕震动参数全部由 View 决定。当前不在 `GameState` 中加入通用事件队列；以后如果出现同一帧多段命中或特效回放需求，再单独设计事件通道。

### 投射物状态

远程型敌人的投射物由 ViewModel 模拟、View 绘制，因此使用独立的 `ProjectileState`：

- `id`：稳定标识，用于旋转、尾迹和消失检测；
- `kind`：投射物的显示种类，当前只有 `ThrownObject`；
- `team`：投射物所属阵营；
- `position`：当前世界坐标，`z` 可以表示抛物线高度；
- `facing`：当前朝向。

`GameState::projectiles` 只保存当前仍然有效的投射物。速度、重力、伤害、碰撞半径、发射者、生命周期和是否命中均留在 ViewModel；颜色、尺寸、旋转角度和尾迹留在 View。

### 补给道具状态

`PickupState` 用于显示当前场景中的补给道具：

- `id`：稳定标识；
- `kind`：`Health` 或 `Energy`；
- `position`：道具世界坐标。

`GameState::pickups` 只包含当前可拾取的道具。恢复数值、生成条件、碰撞范围和掉落概率属于 ViewModel；图标、尺寸、浮动和发光动画属于 View。拾取后的生命值和精力仍通过现有 `HudState` 更新。

### 通用遭遇与波次状态

普通敌人波次和 Boss 战都会使用锁屏、Intro 和清场提示，因此使用统一的 `EncounterState`，不再维护 Boss 专用状态：

- `kind`：`None`、`EnemyWave` 或 `Boss`；
- `phase`：`None`、`Intro`、`Fighting` 或 `Cleared`；
- `currentWave`：当前波次，从 1 开始；
- `totalWaves`：当前遭遇的总波次数；
- `remainingEnemies`：当前遭遇仍需击败的敌人数；
- `introProgress`：Intro 阶段的归一化进度，范围为 `0.0f` 到 `1.0f`。

ViewModel 负责触发遭遇、生成敌人、判断清场、限制玩家移动并推进阶段。View 可以据此显示 `WAVE 1/2`、Boss 出场、GO 提示和锁屏表现。锁屏后的实际镜头坐标继续使用 `MapState::cameraX`。

以下内容不进入 Common：

- 波次触发位置、敌人组成、出生坐标和波次间隔；
- 锁屏左右边界和玩家活动范围；
- Boss 出场动画样式和阶段持续时间；
- 道具奖励和敌人掉落配置。

## View 与 ViewModel 的使用约定

- ViewModel 是 `GameState` 的唯一写入方，负责同步角色、投射物、道具、命中信息和遭遇阶段。
- View 只读取公共状态，不修改 `GameState`，也不根据血量差值、数组长度或道具消失自行判断玩法规则。
- View 可以保存已处理的 `impactRevision`、粒子列表、震动计时、投射物旋转角度和道具浮动动画，这些都属于表现状态。
- 重开游戏时，ViewModel 应清空投射物和道具并重置遭遇与命中状态；View 同时清理本地特效缓存。

## 保留在 Common 的内容

`types.h` 保留 V/VM 传递属性数据会共用的基础值类型。`game_state.h` 保留本游戏的只读状态对象。`notification.h` 保留通知机制。

`actions.h` 已移出 common。`contracts.h` 已删除。这里保留的是具体的 `game_state.h`，而不是试图适配所有模块的通用 `snapshot.h`。
