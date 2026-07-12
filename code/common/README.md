# Common 层接口说明

`common/` 是 View 层和 ViewModel 层之间共享的公共合同。它只定义两层都能理解的数据形状和接口，不写 Qt 界面代码，也不写具体的游戏规则实现。

可以把它理解成两条通道：

- 输入通道：View 把键盘事件翻译成 `GameCommand`，ViewModel 接收并解释成游戏行为。
- 状态通道：ViewModel 生成 `GameSnapshot`，View 只读这个快照并绘制画面。

本项目没有把 Model 单独拆成一个公开层。`GameSimulation` 是 ViewModel 内部的模拟核心，所以本文说的 ViewModel 层包括 `GameViewModel` 和它内部委托的模拟逻辑。

## 总体配合方式

View 层关心“怎么显示、怎么接收输入”。它会使用 common 里的输入类型、快照类型和绑定接口，但不直接修改玩家、敌人、地图等游戏状态。

ViewModel 层关心“游戏规则如何推进”。它接收 common 里的命令，修改内部模拟状态，再把结果整理成 common 里的快照。

Common 层的价值是让两边不用互相依赖具体实现：

- View 不需要知道 `GameSimulation` 里怎么计算碰撞、攻击、刷怪。
- ViewModel 不需要知道 View 用的是 Qt 键盘、Qt Painter，还是以后换成别的前端。
- App 层只负责把 `IGameCommandSink` 和 `IGameSnapshotSource` 绑定起来。

## 类型使用关系总览

| 类型类别 | View 层怎么使用 | ViewModel 层怎么使用 | 配合关系 |
| --- | --- | --- | --- |
| `actions.h` | 把键盘输入翻译成逻辑命令 | 把逻辑命令解释成移动、攻击、暂停、重开 | View 发起，ViewModel 执行 |
| `snapshot.h` | 按完整快照绘制画面、HUD、覆盖层 | 维护并输出当前帧完整状态 | ViewModel 写，View 读 |
| `snapshot.h` / `ActorSnapshot` | 根据角色快照绘制玩家、敌人、Boss | 生成角色状态、位置、朝向、血量等 | 角色快照合同 |
| `snapshot.h` / 流程状态 | 根据游戏阶段切换标题、暂停、胜负画面 | 控制游戏阶段和结算原因 | 流程状态合同 |
| `snapshot.h` / 地图状态 | 根据地图、镜头、GO 提示和进度绘制关卡表现 | 推进关卡、遭遇战、Boss、镜头和锁屏 | 关卡显示合同 |
| `types.h` | 通过快照读取坐标、尺寸、矩形、资源条 | 用这些基础类型计算位置、碰撞、血量等 | 基础值类型合同 |
| `contracts.h` | 通过接口把命令交给 ViewModel，并订阅快照变化 | 实现接口，接收命令并通知变化 | MVVM 绑定合同 |
| `common.h` | 需要完整公共合同时统一包含 | 需要完整公共合同时统一包含 | 便捷入口 |

下面按文件说明每一个公共类型。

## `types.h`：基础值类型

这些类型本身不代表玩法规则，只是跨层共享的基础数据形状。

### `Size`

- View 层：通过 `ActorSnapshot::drawSize` 获取角色绘制宽高。
- ViewModel 层：设置玩家、敌人、Boss 的显示尺寸，并用尺寸辅助内部碰撞计算。
- 配合方式：ViewModel 决定角色在世界中的标准显示大小，View 根据这个大小换算到屏幕上绘制。

### `ResourceBar`

- View 层：读取 `current` 和 `maximum` 绘制血条、精力条、Boss 血条。
- ViewModel 层：维护玩家和敌人的血量、精力，计算攻击消耗、受伤扣血、恢复精力。
- 配合方式：ViewModel 只告诉 View 当前值和最大值，View 决定画成什么样的条形 UI。`ratio()` 让两层都能用统一方式计算百分比。

### `WorldPosition`

- View 层：把 `x`、`laneY`、`z` 转成屏幕坐标；`z` 会影响跳跃和空中攻击的显示高度。
- ViewModel 层：更新角色在关卡中的横向推进、街道纵深、离地高度。
- 配合方式：ViewModel 负责物理和规则结果，View 负责把世界坐标投影成画面位置。

## `actions.h`：输入和命令

这些类型是 View 到 ViewModel 的输入协议。

### `InputAction`

- View 层：把具体按键翻译成逻辑动作，例如左移、右移、轻攻击、重攻击、跳跃、暂停。
- ViewModel 层：不关心具体按键，只关心这些逻辑动作，并决定动作是否能执行、会产生什么效果。
- 配合方式：View 负责“键盘码到动作”，ViewModel 负责“动作到游戏状态变化”。这能避免 ViewModel 依赖 Qt。

### `ButtonState`

- View 层：持续输入用 `Pressed` / `Released`，单次输入用 `Triggered`。
- ViewModel 层：移动类动作会根据 `Pressed` / `Released` 维护持续方向；攻击、跳跃、暂停等动作只响应 `Triggered`。
- 配合方式：同一个 `InputAction` 可以被明确区分为“正在按住”或“一次触发”，解决移动和攻击两类输入的差异。

### `CommandType`

- View 层：发出的命令可以是输入命令，也可以是每帧 tick 请求。
- ViewModel 层：根据命令类型决定是处理玩家输入，还是推进一帧模拟。
- 配合方式：把输入和时间推进统一包装成命令流，ViewModel 入口更简单。

### `InputCommand`

- View 层：由 `InputAction` 和 `ButtonState` 组成，用来描述一次玩家输入。
- ViewModel 层：读取其中的动作和按钮状态，再转成内部移动标记、攻击请求或流程切换。
- 配合方式：这是输入通道里的具体消息体。

### `TickCommand`

- View 层：由定时器产生，携带每帧时间间隔和帧号。
- ViewModel 层：根据 `deltaSeconds` 推进移动、AI、攻击计时、跳跃、精力恢复、镜头等。
- 配合方式：View 提供时间节奏，ViewModel 提供时间内发生的游戏变化。

### `GameCommand`

- View 层：统一发出 `input_command` 或 `tick_command`。
- ViewModel 层：统一接收 `GameCommand`，再按类型分发给输入处理或帧推进。
- 配合方式：它是 View 到 ViewModel 的唯一命令包装。以后增加新命令时，也可以继续沿用这个入口。

## `snapshot.h`：角色快照子结构

这些类型描述“单个场上对象在某一帧公开出来的状态”。它们不是玩家类、敌人类或 Boss 类本身，也不包含 AI、伤害计算、碰撞规则等内部逻辑。

### `Team`

- View 层：根据队伍区分玩家、敌人和中立对象的颜色、血条显示方式等。
- ViewModel 层：判断敌我关系，决定谁可以被攻击、谁是敌人列表里的有效目标。
- 配合方式：ViewModel 产生阵营信息，View 用阵营信息做视觉区分。

### `ActorKind`

- View 层：区分玩家、小怪、Boss、道具、特效，决定不同的绘制风格。
- ViewModel 层：区分不同对象的规则参数，例如 Boss 血量、Boss 速度、小怪生成等。
- 配合方式：同一个 `ActorSnapshot` 可以表示不同种类的对象，避免为玩家、小怪、Boss 各写一套跨层数据。

### `ActorState`

- View 层：根据状态绘制站立、行走、攻击、跳跃、受伤、死亡等不同姿态。
- ViewModel 层：根据输入、AI、碰撞、攻击计时改变角色状态。
- 配合方式：ViewModel 负责状态机，View 负责状态可视化。

### `Facing`

- View 层：根据朝向决定角色面向左还是右。
- ViewModel 层：根据移动方向或攻击方向更新朝向。
- 配合方式：ViewModel 输出朝向，View 用它翻转或调整绘制。

### `ActorSnapshot`

- View 层：从 `GameSnapshot` 中读取它，用位置、尺寸、血量、状态、朝向、是否可见、排序深度等信息绘制角色或特效。
- ViewModel 层：把内部的玩家、敌人、Boss、特效状态整理成这个结构。
- 配合方式：这是 `GameSnapshot` 里的单个对象快照。ViewModel 不把内部对象暴露给 View，只暴露这个只读快照数据。

## `snapshot.h`：游戏流程状态和结算状态

这些类型描述当前游戏处于哪个大阶段，以及胜负结算原因。

### `GamePhase`

- View 层：根据阶段决定显示标题画面、正常游戏画面、暂停遮罩、失败界面或胜利界面。
- ViewModel 层：根据输入和规则切换阶段，例如开始游戏、进入遭遇战锁屏、暂停、Game Over、Win。
- 配合方式：ViewModel 控制流程，View 根据流程状态切换画面。

### `GameOverReason`

- View 层：在失败界面展示失败原因。
- ViewModel 层：在触发失败时记录原因，例如玩家被击败。
- 配合方式：ViewModel 给出语义原因，View 负责把原因显示给玩家。

### `WinReason`

- View 层：在胜利界面展示胜利原因。
- ViewModel 层：在满足胜利条件时记录原因，例如击败 Boss。
- 配合方式：和 `GameOverReason` 类似，用于胜利结算的跨层表达。

### `GameResultSnapshot`

- View 层：绘制 Game Over 或 Win 结算界面时读取用时、击败数量和胜负原因。
- ViewModel 层：在游戏结束时写入结算结果。
- 配合方式：结算信息集中在一个结构中，避免 View 从多个内部规则对象里拼数据。

## `snapshot.h`：地图显示和推进比例

这些类型描述 View 需要画出来的地图状态：镜头在哪里、街道范围是什么、GO 提示是否显示、玩家推进了多少。

### `MapSnapshot`

- View 层：根据地图宽度、视口大小、镜头位置、街道上下边界绘制背景、街道、角色位置和 GO 提示。
- ViewModel 层：更新镜头、世界宽度、可移动边界和 GO 提示；锁屏原因留在内部状态。
- 配合方式：ViewModel 决定世界和镜头的当前状态，View 用这些数据把世界画到屏幕上。

### `GameSnapshot::progressRatio`

- View 层：用它绘制底部关卡进度条。
- ViewModel 层：根据玩家在地图中的横向位置更新它。
- 配合方式：流程控制字段留在 ViewModel 内部，快照只暴露 View 真正需要画出来的进度比例。

## `snapshot.h`：完整游戏快照

这些类型是状态通道的核心。View 理论上只依赖 `GameSnapshot` 就可以完成一帧绘制。

### `HudSnapshot`

- View 层：集中绘制玩家血量、精力、Boss 血条、连招数、疲劳状态等 HUD。
- ViewModel 层：把玩家、Boss、连招、精力恢复等内部状态整理成 HUD 专用数据。
- 配合方式：HUD 不需要 View 自己从角色列表里到处找数据，ViewModel 已经整理好。

### `GameSnapshot`

- View 层：每帧读取快照中的 `phase`、`map`、`player`、`enemies`、`effects`、`hud`、`result` 等字段完成绘制。
- ViewModel 层：持有当前快照，命令和 tick 处理后更新快照，并在变化时通知 View。
- 配合方式：这是 ViewModel 到 View 的主要输出。它保证数据流是单向的：ViewModel 生成状态，View 只读状态。

## `contracts.h`：MVVM 绑定接口

这些类型让 View 和 ViewModel 通过接口连接，而不是直接依赖彼此具体类。

### `BindingCookie`

- View 层：注册变化回调后保存 cookie，销毁绑定时用它取消订阅。
- ViewModel 层：为每个回调分配 cookie，并根据 cookie 移除回调。
- 配合方式：这是订阅关系的句柄，避免 View 销毁后 ViewModel 还调用旧回调。

### `ChangeCallback`

- View 层：传入一个回调函数，当 ViewModel 状态变化时更新快照并重绘。
- ViewModel 层：保存回调列表，在快照变化后逐个通知。
- 配合方式：这是 ViewModel 主动通知 View 的桥梁。当前设计不区分具体变化原因，View 收到通知后直接读取完整 `GameSnapshot`。

### `IGameCommandSink`

- View 层：只把 `GameCommand` 交给这个接口，不需要知道背后是不是 `GameViewModel`。
- ViewModel 层：实现这个接口，作为接收输入和 tick 命令的入口。
- 配合方式：命令从 View 单向流入 ViewModel，接口保证依赖方向稳定。

### `IGameSnapshotSource`

- View 层：通过这个接口读取当前 `GameSnapshot`，并注册变化通知。
- ViewModel 层：实现这个接口，对外提供只读快照和回调管理。
- 配合方式：状态从 ViewModel 单向流向 View，View 只能拿到快照，不能直接操作内部模拟对象。

## `common.h`：公共入口

### `common.h`

- View 层：如果某个文件需要完整 common 合同，可以包含这个总入口。
- ViewModel 层：如果某个文件需要完整 common 合同，也可以包含这个总入口。
- 配合方式：它只是包含其他 common 头文件的便利入口，不定义新的游戏概念。实际开发中如果只用少量类型，优先包含具体头文件，依赖会更清晰。

## 答辩时可以这样总结

Common 层不是新的业务层，而是 View 和 ViewModel 的边界协议。它里面的类型大多分为两类：

- 命令类：`InputAction`、`ButtonState`、`GameCommand` 等，方向是 View 到 ViewModel。
- 快照类：`GameSnapshot`、`ActorSnapshot`、`MapSnapshot`、`HudSnapshot` 等，方向是 ViewModel 到 View。

因此老师问“这些类是否真的被两层使用”时，可以这样回答：

- 像 `GameCommand`、`InputAction`、`IGameCommandSink` 是输入链路的核心，View 生成，ViewModel 消费。
- 像 `GameSnapshot`、`ActorSnapshot`、`GamePhase`、`MapSnapshot` 是显示链路的核心，ViewModel 生成，View 消费。
- 像 `Size`、`ResourceBar`、`WorldPosition` 是嵌在快照里的基础值类型，ViewModel 写入，View 读取。
- 内部编号、遭遇战 id、碰撞矩形等只服务于模拟规则的类型已经留在 ViewModel 层，不放进 Common。

这样的设计让 View 和 ViewModel 的关系保持清楚：View 发命令、读快照；ViewModel 收命令、产快照；Common 负责让这两件事有稳定的数据格式。
