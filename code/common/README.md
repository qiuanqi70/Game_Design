# Common 层接口说明

`common/` 是三层之间共用的“合同”。A、B、C 都可以包含这里的头文件，但这里不写界面、不写游戏逻辑，也不依赖 Qt。


## 文件说明

### `types.h`

最基础的值类型。

- `ActorId`：角色编号。
- `EncounterId`：遭遇战编号。
- `Vec2` / `Vec3`：坐标。
- `Size`：宽高。
- `Rect`：矩形区域，可用于身体盒、攻击盒。
- `ResourceBar`：血量、精力、Boss 血条等资源条。
- `WorldPosition`：世界坐标。

`WorldPosition` 的含义：

```text
x     : 从左到右推进的关卡坐标
laneY : 街道纵深坐标，也就是上下移动
z     : 离地高度，用于跳跃、飞踢、击飞
```

### `actions.h`

输入和命令。

- `InputAction`：玩家动作，例如移动、轻攻击、重攻击、跳跃、重开。
- `ButtonState`：按下、松开、单次触发。
- `TickCommand`：每帧时间推进。
- `GameCommand`：View 传给 ViewModel 的统一命令。

持续移动用 `Pressed` / `Released`，攻击、跳跃、重开用 `Triggered`。

### `game_status.h`

游戏主状态和规则参数。

- `GamePhase`：标题、游戏中、遭遇战锁屏、清屏可前进、暂停、失败、胜利。
- `EncounterState`：遭遇战是否触发、锁定、清完。
- `ScrollLockState`：地图是否被遭遇战或 Boss 战锁住。
- `GameRules`：默认规则参数。
- `GameResultViewData`：结算界面要展示的数据。

### `actor.h`

玩家、小怪、Boss、特效等“场上对象”的公共显示数据。

- `ActorKind::Player`：玩家。
- `ActorKind::Grunt`：普通小怪。
- `ActorKind::Boss`：Boss。
- `ActorState`：站立、行走、攻击、跳跃、受伤、倒地、死亡等状态。
- `SpriteViewData`：动画名字和帧号。
- `CombatBoxViewData`：当前帧生效的攻击盒。
- `ActorViewData`：View 绘制一个角色所需的完整数据。

注意：这里没有 `Player`、`SmallEnemy`、`Boss` 这三个逻辑类。Common 只提供它们的公共显示格式。

### `level.h`

关卡、地图、遭遇战和推图锁。

- `SpawnSpec`：一组刷怪配置。
- `EncounterSpec`：玩家走到某个 `triggerX` 后触发的遭遇战。
- `EncounterViewData`：遭遇战当前运行状态。
- `MapViewData`：镜头、地图大小、街道范围、推图锁、GO 提示。
- `LevelProgressViewData`：关卡进度、Boss 是否出现/击败。

推图锁相关信息主要看：

```cpp
snapshot.map.scrollLock
snapshot.map.showGoIndicator
snapshot.progress.activeEncounterId
```

### `snapshot.h`

View 每帧绘制所需的完整快照。

核心结构是：

```cpp
alleyfist::GameSnapshot
```

它包含：

- `phase`：当前游戏阶段。
- `map`：地图和镜头。
- `progress`：关卡进度。
- `player`：玩家绘制数据。
- `enemies`：小怪和 Boss 绘制数据。
- `effects`：攻击特效、受击特效等。
- `hud`：血条、精力条、Boss 血条、连招信息。
- `result`：胜负结算数据。
- `screenMessage`：屏幕提示文字。

View 应该尽量只依赖 `GameSnapshot` 绘制画面。

### `contracts.h`

App 绑定 View 和 ViewModel 用的接口。

- `CommandHandler`：View 把命令发出去。
- `SnapshotProvider`：View 获取当前快照。
- `ChangeCallback`：ViewModel 通知外部“数据变了”。
- `IGameCommandSink`：能接收命令的对象。
- `IGameSnapshotSource`：能提供快照的对象。

### `common.h`

便捷总入口。

如果某个 `.cpp` 或 `.h` 需要完整 Common 契约，可以直接：

```cpp
#include "common/common.h"
```

如果只需要少量类型，也可以单独包含具体文件，例如：

```cpp
#include "common/actions.h"
#include "common/snapshot.h"
```

## 目前已经覆盖的玩法需求

- 八方向移动：`Direction`、`MovementIntent`
- 轻攻击/重攻击：`InputAction`、`AttackKind`
- 连招：`comboStep`、`comboTimeLeftSeconds`、`ActorState::ComboFinisher`
- 跳跃：`InputAction::Jump`、`WorldPosition::z`、`ActorState::Jump`
- 空中攻击：`ActorState::AirAttack`、`AttackKind::JumpKick`
- 血量：`ResourceBar health`、`hud.playerHealth`
- 精力：`ResourceBar energy`、`hud.playerEnergy`
- 疲劳：`hud.playerExhausted`
- 小怪包抄：`EnemyBehavior::Surround`
- Boss 追击：`EnemyBehavior::BossChase`
- 遭遇战锁屏：`EncounterState`、`ScrollLockState`
- 清屏后 GO 提示：`MapViewData::showGoIndicator`
- Game Over / Win：`GamePhase::GameOver`、`GamePhase::Win`
- 重新开始：`InputAction::Restart`、`CommandType::Restart`

