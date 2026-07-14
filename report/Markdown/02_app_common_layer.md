# 3 App/Common 层分报告

\sectionauthor{苏易文}{3240103466}

## 3.1 层次定位

App/Common 这一部分承担两个不同但相关的职责。

`common/` 是 View 与 ViewModel 之间的公共合同。它不依赖 Qt，也不写具体玩法，只定义两层都能理解的基础值类型、只读游戏状态和通知工具。`app/` 是应用组合根，负责创建 Qt 应用对象、主窗口和 ViewModel，并把 View 与 ViewModel 通过属性、命令和通知三类接口绑定起来。

这两层的共同目标是控制依赖边界：公共状态和通知工具放在 Common，具体对象创建和绑定放在 App，避免 View 和 ViewModel 彼此直接包含对方头文件。

## 3.2 Common 层设计

Common 层主要由四类文件组成：

| 文件 | 作用 |
| --- | --- |
| `types.h` | 定义 `Size`、`WorldPosition`、`ResourceBar` 等基础值类型 |
| `game_state.h` | 定义 `GameState` 及其子结构，作为 View 可读取的状态协议 |
| `notification.h` | 定义 `EventNotification` 与 `EventTrigger`，作为变化通知工具 |
| `common.h` | 便捷入口，统一包含 Common 层公共头文件 |

### 3.2.1 基础值类型

`types.h` 中只保留跨层都会用到、但不携带具体玩法规则的基础属性：

```cpp
struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct ResourceBar {
    int current = 0;
    int maximum = 0;

    float ratio() const noexcept;
    bool is_empty() const noexcept;
};

struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};
```

这些类型可以被 View 和 ViewModel 同时理解。`WorldPosition` 中 `x` 表示横向推进，`laneY` 表示街道纵深，`z` 表示离地高度；View 用它投影到屏幕坐标，ViewModel 用它记录实体位置。

### 3.2.2 游戏状态

`game_state.h` 将 View 需要读取的状态拆成几个层次：

- `GamePhase` 描述当前阶段，包括 `Title`、`Playing`、`Paused`、`GameOver` 和 `Win`。
- `MapState` 描述视口、镜头和街道边界。
- `ActorState` 描述单个角色，包括阵营、种类、位置、绘制尺寸、血量、动作状态、朝向和可见性。
- `HudState` 整理 HUD 专用数据，包括玩家血量、精力、Boss 血条和疲劳状态。
- `GameResultState` 记录结算数据，包括用时和击败数量。
- `GameState` 汇总完整帧状态，作为 View 绘制一帧画面的主要输入。

这样的设计避免 View 从多个内部对象中拼接信息。View 只需要读取 `GameState`，ViewModel 负责把内部规则状态整理成适合显示的数据。

### 3.2.3 事件通知

`notification.h` 提供一个与 Qt 无关的轻量通知工具：

```cpp
using EventNotification = std::function<void(std::uint32_t eventId)>;

class EventTrigger {
public:
    std::uintptr_t add_notification(EventNotification&& notification);
    void remove_notification(std::uintptr_t cookie) noexcept;

protected:
    void fire(std::uint32_t eventId);
};
```

ViewModel 继承 `EventTrigger`，状态变化后调用 `fire(kGameStateChangedEvent)`；View 通过 `MainWindow::get_notification()` 提供一个闭包，收到通知后调用 `GameWidget::update()`。通知只表达“某类事件发生了”，不携带 Qt 对象，也不暴露 ViewModel 内部对象。



## 3.3 App 层设计

`GameApplication` 是 App 层对外暴露的生命周期接口。它使用 PImpl 隐藏 Qt、主窗口和 ViewModel 的具体类型，公共头文件中只保留构造、析构和 `run()`。

实现层中，`GameApplication::Impl` 创建三个核心对象：

```cpp
QApplication qtApp;
GameViewModel viewModel;
MainWindow mainWindow;
```

随后在构造函数中集中完成三类绑定。

第一类是属性绑定：

```cpp
mainWindow.set_game_state(viewModel.get_game_state());
```

第二类是命令绑定：

```cpp
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
```

第三类是通知绑定：

```cpp
viewModel.add_notification(mainWindow.get_notification());
```

App 层知道 `MainWindow` 和 `GameViewModel` 的具体类型，但绑定结果仍然保持 View 与 ViewModel 的单向依赖：View 只持有只读状态指针和命令闭包，不包含 `viewmodel/` 头文件；ViewModel 只保存通知闭包，不知道具体的 `GameWidget`。



## 3.4 小结

App/Common 层建立了稳定边界：Common 把基础值类型、只读状态和通知工具抽象成跨层协议；App 把具体对象创建和三类绑定集中在组合根中。这样整个项目的依赖方向保持清晰，也为 View 和 ViewModel 分别开发提供了基础。
