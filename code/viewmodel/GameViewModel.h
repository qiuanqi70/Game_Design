#pragma once

#include "../common/game_state.h"
#include "../common/notification.h"
#include "GameSimulation.h"
#include "SimulationTypes.h"

#include <cstdint>
#include <memory>

namespace alleyfist::viewmodel {

constexpr std::uint32_t kGameViewStateChangedEvent = 1;

class GameViewModel : public EventTrigger {
public:
    GameViewModel();
    ~GameViewModel();

    GameViewModel(const GameViewModel&) = delete;
    GameViewModel& operator=(const GameViewModel&) = delete;

    // properties
    const GameViewState* get_game_state() const noexcept;

    // commands
    FrameCommand get_tick_command();
    ButtonCommand get_move_left_command();
    ButtonCommand get_move_right_command();
    ButtonCommand get_move_up_command();
    ButtonCommand get_move_down_command();
    Command get_light_attack_command();
    Command get_heavy_attack_command();
    Command get_jump_command();
    Command get_restart_command();
    Command get_confirm_command();
    Command get_pause_command();

    // 启动/停止视图模型（一般由 App 层控制）
    void start();
    void stop();

    // 取得当前实体快照
    const EntityList& entities() const noexcept;

private:
    std::unique_ptr<GameSimulation> m_sim;
    GameViewState m_state;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_paused = false;
    std::uint64_t m_frameIndex = 0;

    void tick(float dt, std::uint64_t frameIndex);
    void sync_state_from_simulation();
    ButtonCommand make_move_command(bool& flag);
    void notify_state_changed();
};

} // namespace alleyfist::viewmodel
