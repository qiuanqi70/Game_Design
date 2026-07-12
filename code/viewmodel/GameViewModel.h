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
    // 接收来自 View 的命令，转发给内部 GameSimulation。
    void handle_command(const GameCommand& command) override;

    // IGameSnapshotSource
    const GameSnapshot& snapshot() const override;
    // 注册快照变化回调，返回 cookie 用于解绑。
    BindingCookie add_change_callback(ChangeCallback callback) override;
    void remove_change_callback(BindingCookie cookie) override;

private:
    // 通知所有已注册的 View 侧回调重新读取快照。
    void notify();

    // 当前对外快照和回调列表。
    GameSnapshot m_snapshot;
    std::vector<std::pair<BindingCookie, ChangeCallback>> m_callbacks;
    BindingCookie m_nextCallbackCookie = 1;
    std::unique_ptr<GameSimulation> m_sim;
};

} // namespace alleyfist
