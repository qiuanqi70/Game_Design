# 3 App/Common 层分报告

\sectionauthor{C}{待填写}

## 3.1 层次定位

App/Common 这一部分承担两个不同但相关的职责。

`common/` 是 View 与 ViewModel 之间的公共合同。它不依赖 Qt，也不写具体玩法，只定义两层都能理解的数据结构和接口。`app/` 是应用组合根，负责创建 Qt 应用对象、主窗口和 ViewModel，并把 View 与 ViewModel 通过 Common 接口绑定起来。

这两层的共同目标是控制依赖边界：公共协议放在 Common，具体对象创建放在 App，避免 View 和 ViewModel 彼此直接包含对方头文件。

## 3.2 Common 层设计

Common 层主要由四类文件组成：

| 文件 | 作用 |
| --- | --- |
| `types.h` | 定义 `Size`、`WorldPosition`、`ResourceBar` 等基础值类型 |
| `actions.h` | 定义 `InputAction`、`ButtonState`、`GameCommand`，作为输入通道协议 |
| `snapshot.h` | 定义 `GameSnapshot` 及其子结构，作为状态通道协议 |
| `contracts.h` | 定义 `IGameCommandSink`、`IGameSnapshotSource` 和回调绑定类型 |

### 3.2.1 输入命令

输入命令将 Qt 键盘事件抽象为游戏动作：

```cpp
enum class InputAction {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    LightAttack,
    HeavyAttack,
    Jump,
    Restart,
    Confirm,
    Pause
};
```

`ButtonState` 区分持续输入和瞬时输入。移动类命令使用 `Pressed` / `Released`，攻击、跳跃、暂停、确认等命令使用 `Triggered`。这样 ViewModel 可以自然地区分“正在移动”和“执行一次动作”。

`GameCommand` 再把输入和 Tick 统一包装起来。View 每帧发出的时间推进也走同一个命令入口，从而让 ViewModel 对外只有一个 `handle_command` 方法。

### 3.2.2 状态快照

`snapshot.h` 将可显示状态拆成几个层次：

- `ActorSnapshot` 描述单个角色或对象，包括阵营、种类、位置、尺寸、血量、精力、状态、朝向和可见性。
- `MapSnapshot` 描述地图和镜头，包括世界宽度、视口大小、街道边界、锁屏边界和 GO 提示。
- `HudSnapshot` 整理 HUD 专用数据，包括玩家血量、精力、Boss 血条、连击和疲劳状态。
- `GameResultSnapshot` 记录结算数据，包括胜负原因、用时和击败数量。
- `GameSnapshot` 汇总完整帧状态，作为 View 绘制一帧画面的全部输入。

这样的设计避免 View 从多个内部对象中拼接信息。View 只需要读取快照，ViewModel 负责把内部规则状态整理成适合显示的数据。

### 3.2.3 绑定接口

`contracts.h` 是 MVVM 边界的核心：

```cpp
class IGameCommandSink {
public:
    virtual ~IGameCommandSink() = default;
    virtual void handle_command(const GameCommand& command) = 0;
};

class IGameSnapshotSource {
public:
    virtual ~IGameSnapshotSource() = default;
    virtual const GameSnapshot& snapshot() const = 0;
    virtual BindingCookie add_change_callback(ChangeCallback callback) = 0;
    virtual void remove_change_callback(BindingCookie cookie) = 0;
};
```

View 只依赖这两个接口：输入发给 `IGameCommandSink`，显示状态从 `IGameSnapshotSource` 获取。ViewModel 实现这两个接口，但不需要知道具体的 `GameWidget`。

## 3.3 App 层设计

`GameApplication` 是 App 层对外暴露的生命周期接口。它使用 PImpl 隐藏 Qt、主窗口和 ViewModel 的具体类型，公共头文件中只保留构造、析构和 `run()`。

实现层中，`GameApplication::Impl` 创建三个核心对象：

```cpp
QApplication qtApp;
GameViewModel viewModel;
MainWindow mainWindow;
```

随后调用：

```cpp
mainWindow.bind(viewModel, viewModel);
```

这里第一个 `viewModel` 作为 `IGameCommandSink` 接收命令，第二个 `viewModel` 作为 `IGameSnapshotSource` 提供快照。App 层知道具体类型，但绑定仍通过公共接口完成，没有让 View 直接依赖 ViewModel 的类定义。

## 3.4 关键实现细节

### 3.4.1 PImpl 控制公共接口

`GameApplication.h` 没有包含 Qt 头文件，也没有暴露 `MainWindow` 或 `GameViewModel`。这让 App 的公共接口保持轻量，减少上层包含成本，也避免外部代码绕过组合根直接访问内部对象。

### 3.4.2 Common 不泄漏模拟细节

Common 只放 View 需要理解的字段。例如 `WorldPosition` 包含 `x`、`laneY` 和 `z`，因为 View 需要用这些值把角色投影到屏幕上；而攻击盒、敌人冷却、遭遇战 ID 和滚动锁枚举都没有放进 Common，而是保留在 ViewModel 的 `SimulationTypes.h` 和 `GameSimulation` 中。

### 3.4.3 App 不承担业务逻辑

App 层只负责启动和绑定，不处理键盘、不绘制画面、不计算攻击伤害。这个边界使后续扩展时不会把初始化代码和业务代码混在一起。

## 3.5 小结

App/Common 层的核心贡献不在于复杂算法，而在于建立稳定边界。Common 把命令和快照抽象成跨层协议，App 把具体对象创建集中在组合根中。这样整个项目的依赖方向保持清晰，也为 View 和 ViewModel 分别开发提供了基础。
