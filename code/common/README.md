# Common 层接口说明

`common/` 保存 View 与 ViewModel 都需要理解的公共契约，包括基础值类型、轻量通知工具，以及本游戏用于属性绑定的只读状态快照。它不放 Qt 类型，不放具体按键动作，也不放游戏规则命令。

## 当前文件

| 文件 | 职责 |
| --- | --- |
| `types.h` | 跨层共享的基础值类型，例如尺寸、世界坐标和资源条。 |
| `notification.h` | `std::function` 形式的事件通知工具。 |
| `game_state.h` | 本游戏的公共状态快照，例如 `GameSnapshot`、`ActorSnapshot` 和 `MapSnapshot`。 |
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

## 属性绑定

老师示例里没有一个通用 `snapshot.h`。Plane 里 View 绑定的是 `const AirMap*`，Book 里 View 绑定的是 `serial/name/summary/price` 指针，Meitu 里 View 绑定的是图片指针。属性变化时只通过通知 id 告诉 View 哪个属性变了。

- `types.h` 放最基础的值类型，例如 `Size`、`WorldPosition`、`ResourceBar`。
- `game_state.h` 放本游戏当前需要跨层共享的具体属性容器，例如 `GameSnapshot`、`ActorSnapshot`、`HudSnapshot`。
- 这些类型不带 `View` 前缀，因为它们属于 Common 公共状态契约，不是 View 层私有结构。

## 保留在 Common 的内容

`types.h` 保留 V/VM 传递属性数据会共用的基础值类型。`game_state.h` 保留本游戏的只读状态快照。`notification.h` 保留通知机制。

`actions.h` 已移出 common。`contracts.h` 已删除。这里保留的是具体的 `game_state.h`，而不是试图适配所有模块的通用 `snapshot.h`。
