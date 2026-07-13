# Common 层接口说明

`common/` 只保存 View 层和 ViewModel 层都需要理解的公共数据形状，以及不依赖具体框架的轻量通知工具。它不放 Qt 类型，不放具体按键动作，也不放游戏规则命令。

## 当前文件

| 文件 | 职责 |
| --- | --- |
| `types.h` | 跨层共享的基础值类型，例如尺寸、世界坐标和资源条。 |
| `snapshot.h` | ViewModel 输出给 View 的只读显示快照。 |
| `notification.h` | `std::function` 形式的事件通知工具。 |
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

## 显示快照

`snapshot.h` 不再定义 `GamePhase`、`GameOverReason`、`WinReason`、`ActorKind`、`ActorState`、`Team` 或 `GameSnapshot`。这些类型带有明显的游戏规则含义，应放在 ViewModel 或更具体的业务层。

现在 common 只定义中性的显示数据：

```cpp
struct FrameSnapshot;
struct ObjectSnapshot;
struct MeterSnapshot;
struct TextSnapshot;
```

`ObjectSnapshot` 使用 `visualId`、`poseId`、`layerId` 这类数字 id 表达“画哪个对象、用哪个姿态、在哪一层画”。这些 id 的枚举和含义由负责 ViewModel/View 的代码定义，common 层只负责承载数据。

## 保留在 Common 的内容

`types.h` 和 `snapshot.h` 仍然属于 common，因为 View 要读取这些基础显示数据，ViewModel 要生成这些基础显示数据。

`actions.h` 已移出 common。`contracts.h` 已删除，因为旧的 `IGameCommandSink` / `IGameSnapshotSource` 接口会被 `std::function` 命令绑定和 `std::function` 事件通知替代。
