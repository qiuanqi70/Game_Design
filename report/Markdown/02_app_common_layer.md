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



## 3.4 技术难点的克服

App/Common 层位于多个模块的交界处，其技术难点不仅在于完成自身代码，还在于建立一套能被 View 和 ViewModel 共同接受并长期使用的公共边界。开发过程中遇到的主要挑战及协作解决过程如下。

### 3.4.1 公共契约的职责收敛

项目初期，Common 层曾包含较多带有业务含义的类型，状态结构的命名和字段组织也经历了多次调整。例如，部分类型先后使用过 `ViewData`、`Snapshot` 和 `State` 等命名，输入动作、状态快照和变化比较逻辑也一度被放入公共层。这样的设计虽然能快速连接各模块，但会使 Common 同时承担 View 输入、ViewModel 规则和数据同步等职责。一旦公共结构发生变化，View 的绘制代码和 ViewModel 的状态转换代码都需要同步修改，放大了接口变更的影响范围。

为解决这一问题，App/Common、View 和 ViewModel 三位成员共同梳理了各层真正需要共享的信息。View 层列出绘制一帧所需的位置、尺寸、血量、动作状态和 HUD 数据，ViewModel 层确认哪些模拟参数应继续保留在内部，App/Common 层再据此精简公共接口。经过多轮讨论与适配，Common 最终收敛为 `types.h`、`game_state.h` 和 `notification.h` 三类稳定内容：基础值类型、只读显示状态和轻量事件通知；移动速度、攻击范围、Boss 触发条件等规则数据则保留在 ViewModel 内部。团队还通过 README、代码注释和 Git 提交同步接口变化，使各模块能够在统一契约下继续开发。虽然前期调整带来了一定返工，但最终显著降低了跨层耦合，也让后续新增状态字段时能够更快判断其所属层次。

### 3.4.2 绑定机制的解耦与统一

第二个难点是如何在不让 View 和 ViewModel 直接依赖彼此的前提下完成交互。早期方案倾向于使用 Qt 的 signal/slot 和额外绑定器连接两层，这种方式实现方便，但会让绑定过程依赖 Qt 类型，并容易把界面事件、业务命令和对象生命周期混合在一起，不利于保持 ViewModel 的纯 C++ 边界。同时，属性读取、输入命令和状态通知采用不同机制，也增加了联调和排查问题的难度。

团队参考课程示例后，统一采用 properties / commands / notification 三类绑定。ViewModel 层负责提供只读 `GameState` 指针和 `std::function` 命令，View 层提供与命令对应的 setter 以及重绘通知闭包，App/Common 层则在 `GameApplication::Impl` 中集中完成全部注入。改造过程中，View 与 ViewModel 成员先共同核对移动、攻击、暂停、确认和重置等命令的名称与参数，再由 App 层逐项连接并进行联合编译和运行验证。最终，View 不需要包含 ViewModel 头文件，ViewModel 也不需要依赖 Qt 控件；若绑定出现遗漏，只需检查组合根中的集中绑定代码，问题定位比原先更加直接。

### 3.4.3 跨模块联调与版本冲突处理

App/Common 的改动通常会同时影响 View 和 ViewModel，因此第三个难点是控制并行开发中的编译错误和合并冲突。尤其在公共类型重命名、状态字段重组和绑定接口调整期间，不同分支可能仍引用旧接口，单个模块能够编译并不代表合并后的完整程序可以正常运行。此外，属性指针、命令闭包和通知回调只有在 App 层全部组装后才能验证完整的数据流，问题往往集中暴露在最终联调阶段。

对此，团队按照“先稳定公共契约，再分别适配，最后集中集成”的顺序推进：App/Common 层先提交接口调整并说明变化，View 和 ViewModel 成员分别通过全局搜索清理旧类型和旧函数，随后在主分支完成合并与冲突处理。联调时，三位成员沿着“键盘输入—命令回调—规则推进—`GameState` 同步—通知重绘”的数据流逐段检查，而不是只在各自模块内部排查。Git 提交历史保留了公共边界精简、耦合修复和合并冲突处理的过程，出现回归时可以快速比较修改前后的接口。通过这种分层排查和共同验证，团队最终打通了标题、游戏、暂停、失败、胜利和重开等完整流程，也积累了在多人协作中先约定接口、再并行实现、最后统一集成的实践经验。

## 3.5 小结

App/Common 层建立了稳定边界：Common 把基础值类型、只读状态和通知工具抽象成跨层协议；App 把具体对象创建和三类绑定集中在组合根中。这样整个项目的依赖方向保持清晰，也为 View 和 ViewModel 分别开发提供了基础。
