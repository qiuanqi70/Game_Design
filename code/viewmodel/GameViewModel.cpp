#include "GameViewModel.h"

#include "GameSimulation.h"

#include <algorithm>

namespace alleyfist {

// ViewModel 接收 View 的命令，委托 GameSimulation 处理玩法逻辑，
// 再把最新 GameSnapshot 暴露给 View。View 收到通知后读取完整快照重绘。

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    m_snapshot = m_sim->snapshot();
}

GameViewModel::~GameViewModel() = default;

void GameViewModel::handle_command(const GameCommand& command)
{
    if (command.type == CommandType::Tick) {
        // Tick 命令推进模拟并刷新快照，随后通知 View 重读完整状态。
        m_sim->step(command.tick.deltaSeconds, command.tick.frameIndex);
        m_snapshot = m_sim->snapshot();
        notify();
        return;
    }

    // 输入命令交给 Simulation 缓存，真正的玩法处理会在 step 中统一执行。
    m_sim->handle_command(command);
}

const GameSnapshot& GameViewModel::snapshot() const
{
    return m_snapshot;
}

BindingCookie GameViewModel::add_change_callback(ChangeCallback callback)
{
    if (!callback) return 0;

    // cookie 用于之后精确解绑回调，避免 View 生命周期结束后仍被通知。
    const BindingCookie cookie = m_nextCallbackCookie++;
    m_callbacks.emplace_back(cookie, std::move(callback));
    return cookie;
}

void GameViewModel::remove_change_callback(BindingCookie cookie)
{
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
                       [cookie](const auto& item) {
                           return item.first == cookie;
                       }),
        m_callbacks.end());
}

void GameViewModel::notify()
{
    // 拷贝一份回调列表，避免回调过程中增删绑定影响本轮遍历。
    const auto callbacks = m_callbacks;
    for (const auto& item : callbacks) {
        if (item.second) {
            item.second();
        }
    }
}

} // namespace alleyfist
