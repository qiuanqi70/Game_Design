#include "GameViewModel.h"

namespace alleyfist::viewmodel {

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    bind_commands();
}

GameViewModel::~GameViewModel() = default;

void GameViewModel::bind_commands()
{
    moveLeft = [this]() { if (m_sim) m_sim->player_move(-10.0f, 0.0f); };
    moveRight = [this]() { if (m_sim) m_sim->player_move(10.0f, 0.0f); };
    moveUp = [this]() { if (m_sim) m_sim->player_move(0.0f, -10.0f); };
    moveDown = [this]() { if (m_sim) m_sim->player_move(0.0f, 10.0f); };
    attack = [this]() { if (m_sim) m_sim->player_attack(); };
    restart = [this]() { if (m_sim) m_sim->reset(); };
}

void GameViewModel::start()
{
    if (!m_sim) return;
    m_sim->start();
    // 注册时钟回调，视图层可以驱动仿真步进也可以由内部定时器驱动
    m_tickCookie = m_sim->add_tick_listener([this](float dt) { on_tick(dt); });
}

void GameViewModel::stop()
{
    if (!m_sim) return;
    m_sim->remove_tick_listener(m_tickCookie);
    m_sim->stop();
    m_tickCookie = 0;
}

const EntityList& GameViewModel::entities() const noexcept
{
    return m_sim->entities();
}

void GameViewModel::on_tick(float dt)
{
    // 当前版本仅在仿真内部处理状态，View 可通过轮询 `entities()` 获取快照并重绘。
    (void)dt; // 占位：未来可在这里进行事件聚合或属性通知
}

} // namespace alleyfist::viewmodel
