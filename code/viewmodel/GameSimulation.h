#pragma once

#include "../common/game_state.h"
#include "SimulationTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace alleyfist::viewmodel {

class GameSimulation {
public:
    GameSimulation();
    ~GameSimulation();

    // 注册/移除时钟监听器（返回 cookie）
    std::uintptr_t add_tick_listener(std::function<void(float)>&& cb);
    void remove_tick_listener(std::uintptr_t cookie) noexcept;

    // 启停与单步推进（由 ViewModel 或外部时钟驱动）
    void start();
    void stop();
    void step(float deltaSeconds);

    // 访问当前实体快照
    const EntityList& entities() const noexcept { return m_entities; }

    // 控制接口（由 ViewModel 的命令绑定）
    void player_move(float dx, float dy) noexcept;
    void player_attack() noexcept;
    void reset() noexcept;

private:
    std::vector<std::function<void(float)>> m_tickListeners;
    EntityList m_entities;
    bool m_running = false;
    int m_nextId = 1;

    void notify_tick(float dt);
    void populate_initial_state();
    void simulate_ai(float dt);
};

} // namespace alleyfist::viewmodel
