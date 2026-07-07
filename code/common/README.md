# Common Layer

Common 是 A(View)、B(ViewModel)、C(App) 都可以依赖的公共契约层。

## 边界

- Common 不包含 FLTK 头文件。
- Common 不实现绘制、AI、碰撞、动画推进等业务逻辑。
- View 只把输入转成 `GameCommand`，并根据 `GameSnapshot` 绘制。
- ViewModel 只处理 `GameCommand`，并暴露 `GameSnapshot`。
- App 负责把 View 的 `CommandHandler` 和 ViewModel 的 `SnapshotProvider` / `ChangeCallback` 绑定起来。

## 主要文件

- `types.h`: 坐标、矩形、血条/精力条、ActorId 等基础类型。
- `actions.h`: 输入动作、命令类型、Tick 命令、八方向移动意图。
- `game_status.h`: 游戏阶段、胜负原因、推图锁状态、基础规则参数。
- `actor.h`: 玩家、杂兵、Boss、攻击盒、动画帧等 View 数据。
- `level.h`: 关卡、遭遇战、刷怪、GO 提示和镜头滚动数据。
- `snapshot.h`: View 每帧绘制所需的完整状态快照。
- `contracts.h`: App 绑定 View 与 ViewModel 时使用的函数类型和抽象接口。

## 约定流程

1. View 捕获键盘或鼠标事件。
2. View 创建 `GameCommand`，例如 `GameCommand::input_command(InputAction::LightAttack, ButtonState::Triggered)`。
3. App 把命令转交给 ViewModel。
4. ViewModel 更新内部状态，并触发 `ChangeCallback`。
5. View 重新读取 `GameSnapshot` 并绘制。
