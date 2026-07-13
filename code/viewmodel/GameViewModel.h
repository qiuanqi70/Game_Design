#pragma once

#include "SimulationTypes.h"
#include "GameSimulation.h"
#include <memory>

namespace alleyfist::viewmodel {

class GameViewModel {
public:
    GameViewModel();
    ~GameViewModel();

    // 命令（直接暴露给 view 层进行绑定）
    Command moveLeft;
    Command moveRight;
    Command moveUp;
    Command moveDown;
    Command attack;
    Command restart;

    // 启动/停止视图模型（一般由 App 层控制）
    void start();
    void stop();

    // 取得当前实体快照
    const EntityList& entities() const noexcept;

private:
    std::unique_ptr<GameSimulation> m_sim;
    std::uintptr_t m_tickCookie = 0;

    void bind_commands();
    void on_tick(float dt);
};

} // namespace alleyfist::viewmodel
