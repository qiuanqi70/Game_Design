# 4 View 层分报告

\sectionauthor{A}{待填写}

## 4.1 层次定位

View 层负责“玩家看到什么”和“玩家输入了什么”。它包含主窗口 `MainWindow` 和游戏画布 `GameWidget`，主要使用 Qt 的窗口、定时器、键盘事件和 `QPainter` 完成界面工作。

按照 MVVM 分工，View 层只依赖 Common 层的 `GameCommand` 和 `GameSnapshot`，不直接包含 `GameViewModel`，也不修改玩家、敌人、Boss、地图等内部状态。所有游戏规则结果都由 ViewModel 通过快照提供。

## 4.2 主窗口与绑定

`MainWindow` 继承自 `QMainWindow`，内部创建 `GameWidget` 作为中心控件，并提供：

```cpp
void bind(IGameCommandSink& commandSink,
          IGameSnapshotSource& snapshotSource);
```

实际绑定由 `GameWidgetBinding` 完成。它做了三件事：

1. 将 `GameWidget::commandGenerated` 转发给 `IGameCommandSink::handle_command`。
2. 将 `GameWidget::tickRequested` 包装成 `GameCommand::tick_command` 后转发给 ViewModel。
3. 向 `IGameSnapshotSource` 注册变化回调，收到通知后调用 `GameWidget::updateSnapshot` 并触发重绘。

绑定器析构时会通过 `BindingCookie` 注销回调，避免 View 销毁后 ViewModel 仍然调用旧回调。

## 4.3 输入处理

View 层将具体键盘码映射为 Common 层定义的逻辑动作：

| 按键 | 逻辑动作 |
| --- | --- |
| 方向键 / WASD | 上下左右移动 |
| J / Z | 轻攻击 |
| K / X | 重攻击 |
| Space | 跳跃 |
| R | 重新开始 |
| Enter | 确认 / 开始 |
| P / Esc | 暂停 |

移动键在按下时发送 `Pressed`，松开时发送 `Released`，这样 ViewModel 可以维护持续移动状态。攻击、跳跃、确认、暂停等操作使用 `Triggered`，表示一次性动作。

为了避免按键自动重复导致多次触发，`GameWidget` 使用 `m_triggeredThisPress` 记录本次按下已经触发过的单次动作；只有按键释放后才允许下一次触发。这保证了攻击、跳跃、暂停等操作的输入语义更加稳定。

## 4.4 Tick 与游戏循环

`GameWidget` 内部使用 `QTimer` 以约 60 FPS 的频率触发更新。每次触发时通过 `QElapsedTimer` 计算距离上一帧的真实时间间隔，并将过大的 delta time 限制到 0.1 秒以内，避免调试断点或系统卡顿后一次性推进过多逻辑。

Tick 不在 View 层直接修改状态，而是通过 `tickRequested(deltaSeconds, frameIndex)` 交给 ViewModel。这样 View 层只负责提供时间节奏，真正的游戏规则推进仍然保留在 ViewModel 中。

## 4.5 绘制流程

`paintEvent` 是 View 层的主绘制入口，整体流程如下：

```text
fill black background
if phase == Title:
  drawOverlay
else:
  drawBackground
  drawStreet
  sort actors by laneY/z
  drawActor for player, enemies and effects
  drawHUD
  drawGOIndicator if needed
  drawOverlay for pause/game over/win
```

标题、暂停、失败和胜利界面都通过覆盖层绘制；正常游戏中则先画背景和街道，再按纵深排序绘制角色，最后绘制 HUD 和提示。

## 4.6 世界坐标到屏幕坐标

ViewModel 输出的是世界坐标：

- `x` 表示关卡横向推进位置。
- `laneY` 表示街道纵深位置。
- `z` 表示离地高度，用于跳跃和空中攻击。

View 层通过以下思路转换到屏幕坐标：

```cpp
screenX = (worldX - cameraX) * scaleX;
screenY = (laneY - z) * scaleY;
```

窗口缩放时，`resizeEvent` 和 `paintEvent` 会根据快照中的标准视口宽高更新 `m_scaleX` 与 `m_scaleY`，让游戏画面可以适应不同窗口尺寸。

## 4.7 角色与 HUD 绘制

角色绘制使用 `ActorSnapshot` 中的阵营、种类、状态、朝向、尺寸和资源条。View 层根据不同 `ActorState` 绘制站立、行走、攻击、跳跃、空中攻击、受伤、死亡等姿态；根据 `Facing` 左右翻转角色；根据 `Team` 和 `ActorKind` 区分玩家、普通敌人和 Boss 的颜色。

HUD 绘制集中在 `drawHUD` 中，主要包括：

- 玩家血量和精力条。
- 精力耗尽时的疲劳提示。
- 连击计数。
- Boss 出现时的 Boss 血条。
- 屏幕中央消息。
- 底部关卡进度条。

敌人和 Boss 头顶还会绘制小型血条，帮助玩家判断战斗反馈。

## 4.8 背景与交互表现

当前 View 层实现了夜色背景、远景建筑、街道路面、人行道、路中虚线、GO 闪烁提示、标题渐变文字、暂停遮罩、失败和胜利结算。远景建筑使用视差滚动，位置变化慢于镜头，增强横向推进时的空间感。

这些表现层效果都只依赖快照，不反向改变游戏规则。例如 GO 提示只读取 `map.showGoIndicator`，是否清屏解锁由 ViewModel 决定。

## 4.9 小结

View 层的核心贡献是把 Common 快照转化为可玩的 Qt 界面，同时严格保持自身职责边界。输入被翻译成逻辑命令，时间被翻译成 Tick，状态被翻译成绘制结果。由于 View 不直接操作规则对象，后续 ViewModel 继续扩展玩法时，View 只需要响应新增的快照字段或状态枚举即可。

