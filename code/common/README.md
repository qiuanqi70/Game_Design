# Common 层接口说明

`common/` 只保存 View 层和 ViewModel 层都需要理解的公共数据形状，以及不依赖具体框架的轻量通知工具。它不放 Qt 类型，不放具体按键动作，也不放游戏规则命令。

## 当前文件

| 文件 | 职责 |
| --- | --- |
| `types.h` | 跨层共享的基础值类型，例如尺寸、世界坐标和资源条。 |
| `snapshot.h` | ViewModel 输出给 View 的只读游戏快照。 |
| `notification.h` | 函数指针 + 上下文指针形式的事件通知工具。 |
| `common.h` | 便捷入口，统一包含上述 common 头文件。 |

## 命令模式

命令不再放在 common 层。具体输入动作、tick 命令和玩法命令应由负责 ViewModel 的代码定义。

命令绑定建议参考老师 Plane 示例，使用 `std::function`：

```cpp
std::function<void(int)> get_move_command();
```

View 持有这个函数对象，在 UI 事件发生时调用它；ViewModel 返回捕获自身的 lambda，在 lambda 里执行对应业务逻辑。

## 事件通知

事件通知使用 callback 方式，而不是 Qt 信号槽或 `std::function` 通知列表。

`notification.h` 提供：

```cpp
using EventNotification = void (*)(std::uint32_t eventId, void* context);
```

触发者通过 `set_notification()` 保存函数指针和上下文指针，事件发生时调用 `fire(eventId)`。接收者通常提供一个静态函数作为 callback，并通过 `context` 转回自己的实例。

示例：

```cpp
class Receiver {
public:
    static void on_event(std::uint32_t eventId, void* context)
    {
        auto* self = static_cast<Receiver*>(context);
        self->handle_event(eventId);
    }

private:
    void handle_event(std::uint32_t eventId);
};
```

`eventId` 的具体含义由触发者所在层定义，common 层不定义具体属性 id 或玩法事件 id。

## 保留在 Common 的内容

`types.h` 和 `snapshot.h` 仍然属于 common，因为 View 要读取这些数据，ViewModel 要生成这些数据。

`actions.h` 已移出 common。`contracts.h` 已删除，因为旧的 `IGameCommandSink` / `IGameSnapshotSource` 接口会被 `std::function` 命令绑定和 callback 事件通知替代。
