#pragma once

#include "actions.h"
#include "snapshot.h"

#include <functional>

namespace alleyfist {

// contracts.h 是 MVVM 绑定的公共边界：
// View 只把命令交给 IGameCommandSink，只从 IGameSnapshotSource 拿只读快照。
// 这样 View 不需要知道具体 GameViewModel，ViewModel 也不需要知道具体 GameWidget。

// ViewModel 用它说明哪里变了，同时不暴露内部对象。
enum class ChangeReason {
    Snapshot,
    Player,
    Enemies,
    Map,
    Hud,
    Phase,
    Result
};

// App 用这些函数类型把 View 和 ViewModel 绑定起来。
using ChangeCallback = std::function<void(ChangeReason)>;
using CommandHandler = std::function<void(const GameCommand&)>;
using SnapshotProvider = std::function<const GameSnapshot&()>;

// 能接收游戏命令的对象需要实现这个接口。
class IGameCommandSink {
public:
    virtual ~IGameCommandSink() = default;
    virtual void handle_command(const GameCommand& command) = 0;
};

// 能暴露当前绘制快照的对象需要实现这个接口。
class IGameSnapshotSource {
public:
    virtual ~IGameSnapshotSource() = default;
    virtual const GameSnapshot& snapshot() const = 0;
    virtual void set_change_callback(ChangeCallback callback) = 0;
};

} // namespace alleyfist
