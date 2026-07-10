#pragma once

#include "actions.h"
#include "snapshot.h"

#include <cstdint>
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

// View 订阅 ViewModel 通知时拿到 cookie，解绑时用它归还绑定关系。
using BindingCookie = std::uintptr_t;
using ChangeCallback = std::function<void(ChangeReason)>;

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
    virtual BindingCookie add_change_callback(ChangeCallback callback) = 0;
    virtual void remove_change_callback(BindingCookie cookie) = 0;
};

} // namespace alleyfist
