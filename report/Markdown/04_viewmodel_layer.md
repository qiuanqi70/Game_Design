# 5 ViewModel 层分报告

\sectionauthor{B}{待填写}

## 5.1 层次定位

ViewModel 层负责游戏规则和可显示状态的生成。它接收 View 发来的 `GameCommand`，推进内部模拟，再输出 `GameSnapshot` 给 View 绘制。它不依赖 Qt，不读取键盘码，也不调用任何绘图函数。

该层分为两个部分：

- `GameViewModel`：对外门面，实现 `IGameCommandSink` 和 `IGameSnapshotSource`。
- `GameSimulation`：内部模拟核心，维护移动、攻击、敌人、遭遇战、胜负判定等规则。

这种拆分让对外绑定逻辑和内部玩法逻辑分开，也避免了窗口代码与移动、攻击、敌人 AI 等规则直接混杂。

## 5.2 GameViewModel 门面

`GameViewModel` 的职责比较克制：

1. 接收 `GameCommand`。
2. 将输入命令转发给 `GameSimulation`。
3. 将 Tick 命令转化为一次 `step`。
4. 保存最新 `GameSnapshot`。
5. 维护变化回调列表，状态变化后通知 View 更新。

`add_change_callback` 会返回 `BindingCookie`，`remove_change_callback` 根据 cookie 移除回调。`notify()` 复制一份回调列表后逐个调用，减少回调过程中修改列表带来的风险。

## 5.3 游戏阶段状态机

当前使用 `GamePhase` 表示游戏大阶段：

| 阶段 | 含义 |
| --- | --- |
| `Title` | 标题界面，等待确认开始 |
| `Playing` | 正常推进和战斗 |
| `EncounterLocked` | 遭遇战锁屏，必须击败敌人 |
| `ClearToGo` | 清屏后可继续前进，显示 GO 提示 |
| `Paused` | 暂停 |
| `GameOver` | 玩家失败 |
| `Win` | 击败 Boss 后胜利 |

输入处理中，`Confirm` 可从标题、失败、胜利进入新游戏，也可从暂停恢复；`Pause` 可在游玩状态与暂停状态之间切换；`Restart` 可随时重置到游玩状态。非游玩阶段会阻止移动和攻击逻辑继续推进。

## 5.4 移动与跳跃

ViewModel 维护四个移动布尔值：`m_moveLeft`、`m_moveRight`、`m_moveUp`、`m_moveDown`。持续按键通过 `Pressed` 和 `Released` 修改这些状态，`apply_movement` 在每帧根据当前移动状态计算水平位移和街道纵深位移。

横向移动会更新角色朝向，纵向移动会改变 `laneY`。角色位置会被限制在世界边界和街道上下边界内；遭遇战或 Boss 战锁屏时，还会被限制在当前锁屏区域内，防止玩家绕过战斗直接推进。

跳跃通过 `m_jumpActive`、`m_jumpElapsed` 和规则参数 `jumpSeconds`、`jumpHeight` 实现。角色的 `z` 值按正弦曲线变化：

```cpp
z = jumpHeight * sin(pi * progress);
```

这样可以得到上升和下落较自然的抛物线效果。跳跃会消耗精力，精力耗尽时设置疲劳状态。

## 5.5 攻击、碰撞与伤害

攻击输入不会立即直接修改敌人，而是先设置待处理攻击：

- 轻攻击对应 `LightPunch`。
- 重攻击对应 `HeavyStrike`。
- 跳跃时触发攻击对应 `JumpKick`。

`apply_attacks` 根据攻击类型设置 `CombatBox`，再将局部攻击盒转换到世界坐标，与敌人的身体矩形做相交检测。命中后扣除敌人血量，更新敌人状态为 `Hurt` 或 `Dead`，并在击败敌人时增加 `defeatedEnemies` 计数。

攻击同时会更新连击显示和精力消耗。若精力降为 0，则设置 `hud.playerExhausted`，后续攻击会被限制，直到能量恢复到足够轻攻击为止。

## 5.6 敌人 AI 与 Boss 战

`update_enemies` 每帧更新敌人：

- 存活敌人会向玩家横向靠近。
- 敌人会根据玩家 `laneY` 调整纵深位置，形成简单的追击行为。
- 敌人接近玩家且冷却结束时会攻击玩家。
- Boss 使用更高移动速度和更高伤害，并在 HUD 中显示独立血条。

敌人和攻击冷却数组保持同序更新。死亡、不可见或血量为 0 的敌人会被过滤出当前敌人列表。

## 5.7 遭遇战和关卡推进

关卡推进使用触发点和滚动锁控制：

- 玩家到达 `kGruntTriggerX` 后触发普通敌人遭遇战，生成 3 个小怪。
- 玩家到达 `kBossTriggerX` 后触发 Boss 战，生成 Boss。
- 遭遇战开始时，`m_scrollLock` 切换为锁定状态，并设置地图左右边界。
- 场上敌人全部被击败后，普通遭遇战切换为 `ClearToGo` 并显示 GO 提示。
- Boss 被击败后，切换到 `Win`，记录胜利原因和用时。

View 只看到阶段、边界、GO 提示和敌人快照，不需要知道内部的遭遇战 ID 或滚动锁枚举。

## 5.8 快照维护

每帧 `step` 会更新：

- `frameIndex` 和 `elapsedSeconds`。
- 玩家位置、状态、血量、精力。
- 敌人列表和 Boss 状态。
- HUD 中的血量、精力、Boss 血条、连击、疲劳。
- 地图镜头和推进比例。
- 胜负结算数据。

`update_camera` 根据玩家位置计算镜头目标，并限制在世界边界内。锁屏时，镜头也会受到遭遇战区域约束。`update_progress` 根据玩家在世界中的横向位置计算进度比例，并扫描敌人列表决定是否显示 Boss 血条。

## 5.9 小结

ViewModel 层的核心贡献是把游戏规则集中在独立的模拟器中，并把结果整理成 View 可直接使用的快照。当前已经实现横版动作游戏的主要骨架：移动、跳跃、攻击、精力、敌人追击、遭遇战锁屏、Boss 战和胜负结算。后续可以继续扩展更完整的连招、击飞、敌人包抄和关卡配置表，而不需要改变 View 与 ViewModel 的基本通信方式。
