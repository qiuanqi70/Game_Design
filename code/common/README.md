# Common 层接口说明

`common/` 只保存真正跨层通用的基础值类型，以及不依赖具体框架的轻量通知工具。它不放 Qt 类型，不放具体按键动作，不放游戏规则命令，也不预先定义完整快照。

## 当前文件

| 文件 | 职责 |
| --- | --- |
| `types.h` | 跨层共享的基础值类型，例如尺寸、世界坐标和资源条。 |
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

## 属性绑定

老师示例里没有一个通用 `snapshot.h`。Plane 里 View 绑定的是 `const AirMap*`，Book 里 View 绑定的是 `serial/name/summary/price` 指针，Meitu 里 View 绑定的是图片指针。属性变化时只通过通知 id 告诉 View 哪个属性变了。

所以本项目 common 不定义 `GameSnapshot`、`FrameSnapshot` 或 `ObjectSnapshot` 这种大而全的快照。V 和 VM 传数据时仍然需要共同数据类型，只是这些类型应该更薄：

- `types.h` 放最基础的值类型，例如 `Size`、`WorldPosition`、`ResourceBar`。
- 具体属性容器由负责 ViewModel 或对应业务模块的代码定义，例如类似老师 Plane 的 `AirMap`。
- 如果某个具体属性容器确实要同时被 View 和 ViewModel 直接包含，可以再放到 common，但它应该像 `AirMap` 那样小而明确，不要把流程状态机、胜负原因、输入命令都塞进去。

## 保留在 Common 的内容

`types.h` 保留 V/VM 传递属性数据会共用的基础值类型。`notification.h` 保留通知机制。

`actions.h` 已移出 common。`contracts.h` 已删除。`snapshot.h` 也已删除，因为这些内容都应该由更具体的 ViewModel/View 绑定代码定义。
