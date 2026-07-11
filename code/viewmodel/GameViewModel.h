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
    void handle_command(const GameCommand& command) override; //接收来自 View 的命令，转发给 GameSimulation 处理

    // IGameSnapshotSource
    const GameSnapshot& snapshot() const override;
    BindingCookie add_change_callback(ChangeCallback callback) override; //注册回调函数，回调函数用于当游戏快照发生变化时通知 View
    void remove_change_callback(BindingCookie cookie) override; //注销回调函数，因为 View 不再需要接收通知

private:
    void notify_changes(const GameSnapshot& before, const GameSnapshot& after); //比较快照的变化，通知回调函数
    void notify(ChangeReason reason); //通知所有注册的回调函数，说明游戏快照发生了变化

    GameSnapshot m_snapshot; //当前游戏快照，供 View 读取
    //注册的回调函数列表，用于通知的发送，事件通知的实现是通过std::function
    //ChangeCallback是一个函数对象类型，表示回调函数的签名
    std::vector<std::pair<BindingCookie, ChangeCallback>> m_callbacks;
    BindingCookie m_nextCallbackCookie = 1; //下一个回调函数的唯一标识符，用于生成新的 BindingCookie
    std::unique_ptr<GameSimulation> m_sim; //内部的游戏模拟器，处理游戏逻辑和状态更新
};

} // namespace alleyfist
