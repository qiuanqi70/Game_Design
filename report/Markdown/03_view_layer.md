# 4 View 层分报告

\sectionauthor{A}{}

## 4.1 层次定位

View 层负责"玩家看到什么"和"玩家输入了什么"。它由主窗口 `MainWindow`、游戏画布 `GameWidget` 和输入聚合定义 `InputDefs.h` 组成，主要使用 Qt 的窗口 (`QMainWindow`)、定时器 (`QTimer`)、键盘事件 (`QKeyEvent`) 和 `QPainter` 完成界面工作。

按照 MVVM 分工，View 层遵循以下约束：

| 约束 | 说明 |
| --- | --- |
| 只依赖 Common 层 | `#include "../common/game_state.h"` 和 `"../common/notification.h"`，不包含 `viewmodel/` 下的任何头文件 |
| 不修改游戏数据 | 持有 `const GameState*` 只读指针，绘制时直接读取，绝不回写 |
| 命令单向注入 | 所有命令通过 `std::function` setter 从外部注入，View 内部不创建命令对象 |
| 不判断规则对错 | 不写 AI、碰撞、伤害、刷怪、推图等逻辑，这些全部属于 ViewModel |

作为一个 Qt Widgets 之上的薄层，View 把物理输入（键盘码）翻译为已绑定的命令回调，把只读状态（`GameState`）翻译为像素，把时间节奏（60 FPS 定时器）翻译为 tick 通知。

与常见的简单 MVVM 视图不同，游戏 View 还需要处理帧同步与流畅绘制的双重任务——既要保证逻辑帧的稳定递进，又要确保视觉帧的流畅渲染。


## 4.2 文件组织与编译

View 层位于 `code/view/` 目录下，由以下文件组成：

| 文件 | 作用 |
| --- | --- |
| `MainWindow.h` / `MainWindow.cpp` | 主窗口封装，提供属性注入、命令注入和通知注册接口 |
| `GameWidget.h` / `GameWidget.cpp` | 游戏画布，包含输入处理、定时循环和全部绘制逻辑 |
| `InputDefs.h` | View 层内部的输入聚合类型 `MovementIntent` |
| `CMakeLists.txt` | 编译为 `alleyfist_view` 静态库，链接 `alleyfist_common` 和 `Qt6::Widgets`、`Qt6::Gui` |

编译配置精简且明确：

```cmake
add_library(alleyfist_view STATIC
    MainWindow.cpp
    GameWidget.cpp
)

target_link_libraries(alleyfist_view
    PUBLIC
        alleyfist_common
        Qt6::Widgets
        Qt6::Gui
)
```

View 库不链接 `alleyfist_viewmodel`，确保 View 与 ViewModel 之间不存在编译级耦合。所有跨层通信均通过 Common 层定义的 `GameState`、`EventNotification` 和 `std::function` 回调完成。


## 4.3 MainWindow 主窗口

`MainWindow` 继承自 `QMainWindow`，内部创建并持有 `GameWidget` 作为中心控件。它是 View 层对外的入口，App 层通过它完成属性绑定、命令注入和通知注册。

MainWindow 的接口设计直接对应 M-V-VM 三层关系的三种绑定方式：

### 4.3.1 属性绑定（Properties）

```cpp
void set_game_state(const GameState* state) noexcept;
```

App 层将 ViewModel 的只读 `GameState` 指针传入 View。MainWindow 不做任何处理，直接转发给 `GameWidget::set_game_state()`。View 端在每次绘制时通过 `game_state()` 辅助函数解引用该指针读取最新状态。

### 4.3.2 命令注入（Commands）

MainWindow 对外暴露十一个命令 setter，每个对应一种游戏语义动作：

| 方法 | 命令签名 | 用途 |
| --- | --- | --- |
| `set_tick_command` | `std::function<void(float, uint64_t)>` | 帧时间驱动 |
| `set_move_left_command` | `std::function<void(bool)>` | 左移动（按下/松开） |
| `set_move_right_command` | `std::function<void(bool)>` | 右移动 |
| `set_move_up_command` | `std::function<void(bool)>` | 上移动 |
| `set_move_down_command` | `std::function<void(bool)>` | 下移动 |
| `set_primary_action_command` | `std::function<void()>` | 轻攻击 |
| `set_secondary_action_command` | `std::function<void()>` | 重攻击 |
| `set_state_toggle_command` | `std::function<void()>` | 跳跃 |
| `set_reset_command` | `std::function<void()>` | 重新开始 |
| `set_confirm_command` | `std::function<void()>` | 确认/开始 |
| `set_pause_command` | `std::function<void()>` | 暂停/恢复 |

所有命令 setter 都是直接将 `std::function` 转发给 `GameWidget` 的对应 setter，MainWindow 本身不做命令封装。这种透传设计使得 MainWindow 保持极薄的职责——仅作为 App 层到 GameWidget 的命令管道。

### 4.3.3 通知注册（Notification）

```cpp
EventNotification get_notification();
```

该方法返回一个 `EventNotification`（`std::function<void(uint32_t)>`）闭包。闭包捕获 `this`，在被调用时触发 `GameWidget::update()` 引起重绘。App 层在组合根中通过 `viewModel.add_notification(mainWindow.get_notification())` 将闭包注入 ViewModel。

当 ViewModel 完成状态更新后调用 `fire(kGameStateChangedEvent)` 时，这个闭包被执行，GameWidget 随即调度重绘。GameWidget 在 `paintEvent` 中通过已绑定的只读指针读取 ViewModel 的最新状态，完成从"变化通知"到"数据读取"的解耦闭环。

### 4.3.4 整体绑定流程

在 App 层的组合根中，绑定三个步骤按如下顺序执行：

```cpp
mainWindow.set_game_state(viewModel.get_game_state());

mainWindow.set_tick_command(viewModel.get_tick_command());
mainWindow.set_move_left_command(viewModel.get_move_left_command());
mainWindow.set_move_right_command(viewModel.get_move_right_command());
mainWindow.set_move_up_command(viewModel.get_move_up_command());
mainWindow.set_move_down_command(viewModel.get_move_down_command());
mainWindow.set_primary_action_command(viewModel.get_primary_action_command());
mainWindow.set_secondary_action_command(viewModel.get_secondary_action_command());
mainWindow.set_state_toggle_command(viewModel.get_state_toggle_command());
mainWindow.set_reset_command(viewModel.get_reset_command());
mainWindow.set_confirm_command(viewModel.get_confirm_command());
mainWindow.set_pause_command(viewModel.get_pause_command());

viewModel.add_notification(mainWindow.get_notification());
```

这个模式直接参考了课程课件 Plane 示例项目中 `AirApp::initialize()` 的绑定方式：

```cpp
m_main_wnd.get_board().set_map(m_game_viewmodel.get_map());
m_main_wnd.set_next_step_command(m_game_viewmodel.get_next_step_command());
m_game_viewmodel.add_notification(m_main_wnd.get_notification());
```

与 Plane 示例的一致性表明，本项目的 View 层遵循了标准的 MVVM 绑定范式。


## 4.4 GameWidget 游戏画布

`GameWidget` 是 View 层的核心，继承自 `QWidget`，约 550 行实现。它同时承担四个职责：**接收数据**、**收集输入**、**驱动帧循环**和**渲染画面**。

### 4.4.1 数据访问模式

GameWidget 持有 `const GameState* m_gameState` 只读指针，由 App 层在绑定时通过 `set_game_state()` 注入。此外持有一个默认构造的 `GameState m_emptyState` 作为后备——当指针为空（尚未绑定）时使用空状态，避免空指针解引用。

通过私有辅助方法 `game_state()` 统一访问：

```cpp
const GameState& game_state() const noexcept
{
    return m_gameState != nullptr ? *m_gameState : m_emptyState;
}
```

所有绘制方法（`drawBackground`、`drawActor`、`drawHUD` 等）通过 `game_state()` 获取快照引用，再访问其中的 `map`、`player`、`enemies`、`hud`、`phase` 等字段。这种间接访问方式有两个好处：一是集中处理空指针边界条件；二是为后续引入帧间插值等高级特性预留了扩展点。

### 4.4.2 命令注入

GameWidget 持有十一个 `std::function` 成员变量，对应十一种游戏语义命令：

```cpp
std::function<void(float, std::uint64_t)> m_tickCommand;
std::function<void(bool)> m_moveLeftCommand;
std::function<void(bool)> m_moveRightCommand;
std::function<void(bool)> m_moveUpCommand;
std::function<void(bool)> m_moveDownCommand;
std::function<void()> m_primaryActionCommand;
std::function<void()> m_secondaryActionCommand;
std::function<void()> m_stateToggleCommand;
std::function<void()> m_resetCommand;
std::function<void()> m_confirmCommand;
std::function<void()> m_pauseCommand;
```

每个成员都有对应的 public setter（例如 `set_tick_command`、`set_move_left_command` 等）。这些 setter 由 App 层经由 MainWindow 逐层注入 ViewModel 的闭包。键盘事件和定时器回调直接调用这些 `std::function`，不经过任何 Qt 信号中间层。

这样的设计与参考项目 Plane 完全一致：View 持有 ViewModel 给出的命令闭包，在发生输入事件时直接调用它们，形成从 View 到 ViewModel 的直接控制通道。

### 4.4.3 键盘输入处理

GameWidget 重写 `keyPressEvent` 和 `keyReleaseEvent`，将 Qt 物理键码映射为已注入的游戏语义命令。按键映射集中在 `isHandledKey` 静态方法和两个事件处理函数中，换手柄或触屏只需修改这里的映射逻辑。

**移动键处理**采用"状态聚合 + 命令回调"的模式。GameWidget 内部维护 `view::MovementIntent m_movement` 结构体，其中包含四个方向布尔值 `left`、`right`、`up`、`down`。当用户按下移动键时：

1. 检查对应方向标志是否已处于按下状态，避免重复触发
2. 设置方向标志为 `true`
3. 调用已注入的方向命令回调（如 `m_moveLeftCommand(true)`）

释放移动键时则反向操作——清除方向标志并调用回调（如 `m_moveLeftCommand(false)`）。这种设计将"哪些键被按住"的状态维护在 View 层内部，ViewModel 只需要知道每个方向是激活还是停用，无需关心具体哪个物理按键产生了这种变化。

**单次触发动作**（攻击、跳跃、暂停、确认、重置）使用 `m_triggeredThisPress`（`QSet<int>`）防止同一按键在按住期间产生多次触发。按下时如果该按键不在集合中，则执行命令回调并将按键加入集合；释放时从集合中移除。这保证了攻击、跳跃等操作的输入语义稳定，避免键盘自动重复导致的意外连击。

按键映射关系如下：

| Qt 物理键 | 触发的命令 |
| --- | --- |
| ← / A | `m_moveLeftCommand(true/false)` |
| → / D | `m_moveRightCommand(true/false)` |
| ↑ / W | `m_moveUpCommand(true/false)` |
| ↓ / S | `m_moveDownCommand(true/false)` |
| J / Z | `m_primaryActionCommand()` |
| K / X | `m_secondaryActionCommand()` |
| Space | `m_stateToggleCommand()` |
| R | `m_resetCommand()` |
| Enter | `m_confirmCommand()` |
| P / Esc | `m_pauseCommand()` |

所有命令回调直接从按键事件中调用，不再经过 Qt 的 `signal`/`slot` 机制。这意味着：

- View → ViewModel 的控制通道纯粹由 C++ `std::function` 构成
- Qt 仅作为操作系统事件源（`QKeyEvent`）和渲染后端（`QPainter`），不参与 MVVM 绑定
- 将来替换为其他 GUI 框架（如 SDL、GLFW）时，只需替换 `QKeyEvent` 到命令的映射逻辑，无需修改 ViewModel 或 Common 层

### 4.4.4 Tick 与游戏循环

`GameWidget` 内部使用 `QTimer` 以约 60 FPS（16 ms 间隔）的频率驱动游戏逻辑帧。每次定时器触发时：

1. 通过 `QElapsedTimer::restart()` 获取自上一帧以来的真实时间间隔
2. 将 delta time 限制在 0.1 秒以内，防止断点调试或系统卡顿后一次性推进过多逻辑
3. 递增帧序号
4. 累积 GO 闪烁计时（用于 `showGoIndicator` 时的提示闪烁效果）
5. 调用已注入的 `m_tickCommand(clampedDt, m_frameIndex)`

与旧版不同，Tick 不再通过 Qt `signal` 发出 `tickRequested`，而是直接在定时器 lambda 中调用 `std::function` 回调。QTimer 在此仅作为操作系统定时事件的来源——Qt 的信号机制仍然用于将定时器事件从 Qt 事件循环传递到 GameWidget 的回调 lambda，但这是 Qt 内部的事件传递机制，与 MVVM 跨层绑定无关。

这种设计使得 View 层的定时器成为一个"纯 View 内部"的时钟源——定时器启动、停止、频率控制全部在 GameWidget 内部完成，对外只暴露 `setRunning(bool)` 用于外部启停控制。

### 4.4.5 坐标转换

ViewModel 输出的是游戏世界坐标：

- `x`：关卡横向推进位置（0 ~ 3000）
- `laneY`：街道纵深位置（300 ~ 500）
- `z`：离地高度，用于跳跃和空中攻击（0 ~ 90）

View 层通过以下变换将世界坐标映射到屏幕像素：

```cpp
screenX = (worldX - cameraX) * scaleX;
screenY = (laneY - z) * scaleY;
```

其中 `cameraX` 来自 `game_state().map.cameraX`，由 ViewModel 根据玩家位置和锁屏状态计算。`scaleX` 和 `scaleY` 根据标准视口（960×540）与当前窗口实际尺寸的比例动态计算，在 `resizeEvent` 和 `paintEvent` 中更新，使得游戏画面可以适应不同的窗口大小。

### 4.4.6 绘制管线

`paintEvent` 是 View 层的主绘制入口。整体流程以游戏阶段 `GamePhase` 为分支条件：

```text
填充黑色背景
if phase == Title:
    drawOverlay          → 标题画面
else:
    drawBackground       → 夜空 + 视差建筑
    drawStreet           → 人行道 + 车道 + 标线
    按 laneY+z 排序所有可见角色
    drawActor 逐个绘制  → 玩家、敌人
    drawHUD              → 血条、精力条、连击、进度条
    if showGoIndicator:
        闪烁 "GO ▶" 提示
    if phase == Paused | GameOver | Win:
        drawOverlay      → 半透明遮罩 + 对应文字
```

这种分阶段绘制的方式使得 View 可以自然地支持所有游戏界面，无需额外标志位。

### 4.4.7 背景与场景绘制

**天空**：使用 `QLinearGradient` 绘制从深蓝到地平线亮色的垂直渐变，模拟夜幕下的城市街道氛围。

**远景建筑**：通过硬编码的建筑物位置和尺寸数据，绘制一系列深灰色矩形作为城市剪影。建筑使用视差滚动——位置随镜头移动的速度仅为主场景的 0.3 倍（`parallaxFactor = 0.3f`），增强了横向推进时的空间纵深感。每栋建筑还通过位置哈希绘制明暗交替的窗户网格，避免静态重复感。

**街道**：从 `map.streetTopY` 到 `map.streetBottomY` 分三层：
- 上人行道（15% 高度，浅灰色）
- 车道（70% 高度，深灰色），中央黄色虚线随镜头位置动态偏移
- 下人行道（15% 高度，浅灰色）
- 路缘石和人行道以下的地面填充

所有街道元素均使用 `QPainter::fillRect` 绘制，不依赖外部图片资源，保证在没有图像素材时依然可以完整展示游戏场景。

### 4.4.8 角色绘制

`drawActor` 方法为单个角色绘制完整形象。它调用 `worldToScreenX` 和 `worldToScreenY` 将世界坐标转换为屏幕坐标后，根据 `Facing`（左右朝向）对画布做水平翻转，再按 `ActorState`（站立、行走、攻击、跳跃、受伤、死亡等）绘制不同姿态。

角色图形完全由 `QPainter` 几何图形构成，包括：

- **腿**：根据状态变化——站立时呈静态、行走时交替前后摆动、跳跃时收起。使用裤子颜色和鞋子颜色区分
- **身体**：矩形躯干，颜色由 `Team`（玩家蓝、小怪红、Boss 深红）和 `ActorKind` 决定。受击时变为红色闪烁
- **手臂**：攻击时向前伸出并带拳头方块，重攻击伸出距离更长。空中攻击时以踢腿姿态展示
- **头部**：圆形头部 + 白色眼白 + 黑色瞳孔 + 队色头巾/头发
- **阴影**：角色底部的地面椭圆阴影
- **特效**：死亡状态红色 X 线、受击状态黄色星形粒子

敌人头顶还会绘制小型血条（绿色→黄色→红色渐变），帮助玩家判断战斗反馈。无敌状态的角色按 0.1 秒周期交替显示/隐藏。

### 4.4.9 HUD 绘制

`drawHUD` 将所有界面元素集中在 View 层内部：

- **血条（HP）**：左上角，绿色→黄色→红色渐变填充，标注 `HP` 标签和当前值/最大值
- **精力条（EN）**：HP 条下方，蓝色填充，标注 `EN` 标签和当前值/最大值
- **疲劳警告**：精力耗尽时显示红色 `EXHAUSTED` 文字
- **连击计数**：屏幕中上方，白色 1 HIT → 黄色 2 HIT → 红色 3 HIT
- **Boss 血条**：屏幕顶部居中偏右，Boss 出现时显示，标注 `BOSS` 和血量数值
- **屏幕消息**：中央位置显示 ViewModel 传入的提示文字（如 "Paused"）
- **关卡进度条**：底部居中，金色填充进度，根据 `progressRatio` 或玩家位置/世界宽度计算

所有 HUD 元素的颜色、字体和位置均通过 `m_scaleX` / `m_scaleY` 缩放，保证在不同窗口尺寸下比例一致。

### 4.4.10 GO 提示与覆盖层

**GO 提示**：当 ViewModel 设置 `map.showGoIndicator = true`（清屏解锁后），View 以 0.5 秒亮/0.5 秒灭的节奏在屏幕右上方绘制闪烁的 `GO ▶` 文字。闪烁通过多层描边（3 层不同透明度和宽度的黄色描边 + 白色主体）实现发光效果。

**覆盖层**：`drawOverlay` 根据 `GamePhase` 绘制四种不同的全屏界面：

| 阶段 | 覆盖层内容 |
| --- | --- |
| Title | 深色渐变背景 + 渐变标题 `ALLEY FIST` + 副标题 + 操作说明列表 + 闪烁 `Press ENTER to Start` |
| Paused | 半透明黑色遮罩 + `PAUSED` 大字 + `Press P or ESC to Resume` |
| GameOver | 半透明遮罩 + 红色 `GAME OVER` + 存活时间和击败敌人统计 + 闪烁 `Press R to Restart` |
| Win | 半透明遮罩 + 金色渐变 `YOU WIN!` + 通关时间和击败敌人统计 + 闪烁 `Press R to Restart` |

标题画面的操作说明直接编码在 View 中，与当前的键盘映射一致，既能作为启动界面引导，也便于测试时快速验证输入映射是否正确。


## 4.5 InputDefs.h 输入聚合

`InputDefs.h` 将输入层的类型定义在 View 内部（`namespace alleyfist::view`）：

```cpp
namespace alleyfist::view {

struct MovementIntent {
    bool left  = false;
    bool right = false;
    bool up    = false;
    bool down  = false;

    bool is_moving() const noexcept
    {
        return left || right || up || down;
    }

    void clear() noexcept
    {
        left = right = up = down = false;
    }
};

} // namespace alleyfist::view
```

`MovementIntent` 将四个独立方向标志聚合为一个语义结构。这些方向标志在 `keyPressEvent` / `keyReleaseEvent` 中被设置和清除。`is_moving()` 用于快速判断是否有任何方向键被按下，而具体的移动方向推导可由 ViewModel 在收到各方向的 `bool` 命令后自行完成。

将 `MovementIntent` 放在 `view` 子命名空间而非 Common 层，遵循了"输入设备相关类型不应出现在公共契约中"的架构原则。换手柄、触屏等输入方式时，只需修改 `GameWidget` 的按键映射和 `MovementIntent` 的填充逻辑。


## 4.6 与旧版设计的对比

本项目 View 层的架构经历了两次重要的设计迭代：

### 4.6.1 第一版：Qt 信号驱动的 MVVM

最初版本采用 Qt `signal`/`slot` 作为 MVVM 绑定机制：

```cpp
signals:
    void commandGenerated(const GameCommand& command);
    void tickRequested(float deltaSeconds, std::uint64_t frameIndex);

connect(m_widget, &GameWidget::commandGenerated, this, [this](const GameCommand& command) {
    m_commandSink.handle_command(command);
});
```

这种设计的耦合问题：
- `GameCommand` 需要在 Common 层定义结构体包装输入数据和 Tick 数据
- ViewModel 需要实现 `IGameCommandSink` / `IGameSnapshotSource` 接口
- 绑定器 `GameWidgetBinding` 作为中间人同时持有 View 和 ViewModel 的引用
- Qt 信号机制渗透进 MVVM 绑定路径

### 4.6.2 第二版（当前版本）：std::function 直接注入

参考课程 Plane 示例后，改为 `std::function` 直接回调：

| 对比维度 | 第一版 | 第二版 |
| --- | --- | --- |
| 命令传播 | `emit signal → connect → lambda → handle_command` | `std::function()` 直接调用 |
| 命令粒度 | 一个 `GameCommand` 结构体 + `CommandType` 分发 | 十个独立语义命令，各司其职 |
| 接口契约 | `IGameCommandSink` / `IGameSnapshotSource` 抽象类 | `std::function` setter + `const GameState*` |
| 通知方式 | cookie 回调列表 | `EventTrigger` 的 `add_notification` |
| 绑定器 | 独立的 `GameWidgetBinding` 类 | App 层在主函数中直接注入 |
| 移动输入 | 四个独立 `Pressed`/`Released` | 四个方向 `bool` 命令 + View 内 `MovementIntent` 聚合 |

第二版的关键改进：
1. Qt 仅作为事件源（`QKeyEvent`）和渲染后端（`QPainter`），不参与 MVVM 绑定
2. 命令签名自文档化——`set_move_left_command(std::function<void(bool)>)` 明确表达了"向左移动的开/关"
3. View 层不依赖任何 ViewModel 头文件，只依赖 `common/game_state.h` 和 `common/notification.h`
4. 将来替换 GUI 框架只需改 `keyPressEvent`/`keyReleaseEvent` 和 `paintEvent`，无需动其他层


## 4.7 小结

View 层在本项目中的核心贡献是把 Common 契约转化为可玩的 Qt 游戏界面，同时严格保持自身职责边界：

- **输入**：键盘物理事件被翻译为已注入的命令回调，每个方向独立可追踪
- **时间**：60 FPS 定时器驱动逻辑帧，tick 命令通过 `std::function` 发出
- **状态**：ViewModel 的只读 `GameState` 指针被解释为可视化的场景、角色和 HUD
- **通知**：ViewModel 的 `fire(kGameStateChangedEvent)` 触发 View 重绘，View 在重绘时从指针拉取最新数据

由于 View 不直接操作规则对象，也不包含任何 ViewModel 头文件，后续 ViewModel 继续扩展玩法时，View 只需要响应 `GameState` 中新增的字段或状态枚举即可。这种分层方式使界面迭代（如替换精灵动画为真实图片）和逻辑迭代（如新增敌人类型）可以独立进行，降低了多人协作时的合并冲突概率。

