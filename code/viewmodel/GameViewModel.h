#pragma once

#include "../common/contracts.h"

#include <memory>
#include <utility>
#include <vector>

namespace alleyfist {

class GameSimulation;

// GameViewModel 是 ViewModel 层对外的门面：
// 对 View 暴露命令入口和只读快照源，对内部委托 GameSimulation 处理玩法逻辑。
// 它不包含 QWidget/GameWidget，也不调用任何绘制代码。
class GameViewModel final : public IGameCommandSink, public IGameSnapshotSource {
public:
    GameViewModel();
    ~GameViewModel() override;

    GameViewModel(const GameViewModel&) = delete;
    GameViewModel& operator=(const GameViewModel&) = delete;

    // IGameCommandSink
    void handle_command(const GameCommand& command) override;

    // IGameSnapshotSource
    const GameSnapshot& snapshot() const override;
    BindingCookie add_change_callback(ChangeCallback callback) override;
    void remove_change_callback(BindingCookie cookie) override;

private:
    void notify();

    GameSnapshot m_snapshot;
    std::vector<std::pair<BindingCookie, ChangeCallback>> m_callbacks;
    BindingCookie m_nextCallbackCookie = 1;
    std::unique_ptr<GameSimulation> m_sim;
};

} // namespace alleyfist
