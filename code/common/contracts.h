#pragma once

#include "actions.h"
#include "snapshot.h"

#include <cstdint>
#include <functional>

namespace alleyfist {

// contracts.h 是 MVVM 绑定的公共边界：
// View 只把命令交给 IGameCommandSink，只从 IGameSnapshotSource 拿只读快照。
// 这样 View 不需要知道具体 GameViewModel，ViewModel 也不需要知道具体 GameWidget。

// View 订阅 ViewModel 通知时拿到 cookie，解绑时用它归还绑定关系。
using BindingCookie = std::uintptr_t;
using ChangeCallback = std::function<void()>;

// View → ViewModel
class IGameCommandSink {
public:
    virtual ~IGameCommandSink() = default;
    virtual void handle_command(const GameCommand& command) = 0;
};

// ViewModel → View
class IGameSnapshotSource {
public:
    virtual ~IGameSnapshotSource() = default;
    virtual const GameSnapshot& snapshot() const = 0;
    virtual BindingCookie add_change_callback(ChangeCallback callback) = 0;
    virtual void remove_change_callback(BindingCookie cookie) = 0;
};

} // namespace alleyfist
