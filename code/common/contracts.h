#pragma once

#include "actions.h"
#include "snapshot.h"

#include <functional>

namespace alleyfist {

enum class ChangeReason {
    Snapshot,
    Player,
    Enemies,
    Map,
    Hud,
    Phase,
    Result
};

using ChangeCallback = std::function<void(ChangeReason)>;
using CommandHandler = std::function<void(const GameCommand&)>;
using SnapshotProvider = std::function<const GameSnapshot&()>;

class IGameCommandSink {
public:
    virtual ~IGameCommandSink() = default;
    virtual void handle_command(const GameCommand& command) = 0;
};

class IGameSnapshotSource {
public:
    virtual ~IGameSnapshotSource() = default;
    virtual const GameSnapshot& snapshot() const = 0;
    virtual void set_change_callback(ChangeCallback callback) = 0;
};

} // namespace alleyfist
