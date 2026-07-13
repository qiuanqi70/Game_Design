#pragma once

#include "../common/game_state.h"
#include "../common/notification.h"
#include "GameSimulation.h"
#include "SimulationTypes.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace alleyfist::viewmodel {

constexpr std::uint32_t kGameStateChangedEvent = 1;

class GameViewModel : public EventTrigger {
public:
    GameViewModel();
    ~GameViewModel();

    GameViewModel(const GameViewModel&) = delete;
    GameViewModel& operator=(const GameViewModel&) = delete;

    // properties
    const GameState* get_game_state() const noexcept;

    // commands
    std::function<void(float, std::uint64_t)> get_tick_command();
    std::function<void(bool)> get_move_left_command();
    std::function<void(bool)> get_move_right_command();
    std::function<void(bool)> get_move_up_command();
    std::function<void(bool)> get_move_down_command();
    std::function<void()> get_primary_action_command();
    std::function<void()> get_secondary_action_command();
    std::function<void()> get_state_toggle_command();
    std::function<void()> get_reset_command();
    std::function<void()> get_confirm_command();
    std::function<void()> get_pause_command();

    // 启动/停止视图模型（一般由 App 层控制）
    void start();
    void stop();

    // 取得当前实体状态
    const EntityList& entities() const noexcept;

private:
    std::unique_ptr<GameSimulation> m_sim;
    GameState m_state;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_paused = false;
    bool m_jumpActive = false;
    float m_jumpElapsed = 0.0f;
    float m_attackTimer = 0.0f;
    float m_playerEnergy = 100.0f;
    float m_exhaustedWarningTimer = 0.0f;
    ActorActionState m_attackState = ActorActionState::Idle;

    void tick(float dt);
    void sync_state_from_simulation();
    std::function<void(bool)> make_move_command(bool& flag);
    void begin_attack(ActorActionState actionState, float seconds, float energyCost);
    void reset_actions() noexcept;
    void update_action_timers(float dt);
    bool try_spend_energy(float cost) noexcept;
    bool is_gameplay_active() const noexcept;
    void notify_state_changed();
};

} // namespace alleyfist::viewmodel
