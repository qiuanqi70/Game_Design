#pragma once

#include "../common/game_state.h"
#include "../common/notification.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace alleyfist::viewmodel {

constexpr std::uint32_t kGameStateChangedEvent = 1;

class GameSimulation;
enum class PlayerActionType;

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

private:
    enum class MoveInput {
        Left,
        Right,
        Up,
        Down
    };

    std::unique_ptr<GameSimulation> m_sim;
    GameState m_state;

    void tick(float dt);
    void sync_state_from_simulation();
    std::function<void(bool)> make_move_command(MoveInput input);
    std::function<void()> make_action_command(PlayerActionType action);
    void reset_game();
    bool is_gameplay_active() const noexcept;
    void notify_state_changed();
};

} // namespace alleyfist::viewmodel
