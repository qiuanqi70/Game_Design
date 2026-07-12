#include "GameViewModel.h"

#include "GameSimulation.h"

#include <algorithm>

namespace alleyfist {

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    m_snapshot = m_sim->snapshot();
}

GameViewModel::~GameViewModel() = default;

// ViewModel 接收命令后推进内部模拟，再通过通知列表发出变化通知。
// 这就是 MVVM 中 ViewModel -> View 的通知绑定，不需要 ViewModel 认识具体 View。
void GameViewModel::handle_command(const GameCommand& command)
{
    if (command.type == CommandType::Tick) { //如果是 Tick 命令（每帧更新），推进游戏模拟
        m_sim->step(command.tick.deltaSeconds, command.tick.frameIndex); //通过 GameSimulation 处理 Tick 命令，推进游戏状态
        m_snapshot = m_sim->snapshot(); //更新快照为模拟器的当前状态
        notify();
        return;
    }

    m_sim->handle_command(command); //如果是 Input 命令（玩家输入），转发给 GameSimulation 处理
}

const GameSnapshot& GameViewModel::snapshot() const
{
    return m_snapshot;
}

// 注册回调函数，当游戏快照发生变化时通知 View
BindingCookie GameViewModel::add_change_callback(ChangeCallback callback)
{
    if (!callback) return 0;

    const BindingCookie cookie = m_nextCallbackCookie++;
    m_callbacks.emplace_back(cookie, std::move(callback)); //将回调函数和唯一标识符存储在回调列表中
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

// 通知所有注册的回调函数，说明游戏快照发生了变化
void GameViewModel::notify()
{
    const auto callbacks = m_callbacks;
    for (const auto& item : callbacks) {
        if (item.second) {
            item.second();
        }
    }
}

} // namespace alleyfist
