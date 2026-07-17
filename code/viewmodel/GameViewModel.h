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

/// @brief ViewModel 层对 App/View 暴露的唯一游戏状态入口。
///
/// GameViewModel 按 MVVM 思路把内部规则模型 GameSimulation 转换成 Common
/// 层定义的 GameState。View 只能读取 get_game_state() 返回的快照，并通过
/// get_xxx_command() 取得命令回调；这样 View 不需要知道敌人 AI、碰撞、刷怪等
/// 规则细节，也不会直接修改游戏状态。
class GameViewModel : public EventTrigger {
public:
    GameViewModel();
    ~GameViewModel();

    GameViewModel(const GameViewModel&) = delete;
    GameViewModel& operator=(const GameViewModel&) = delete;

    // properties: App 绑定给 View 的只读显示状态。
    const GameState* get_game_state() const noexcept;

    // commands: App 把这些命令注入 View，View 只负责触发输入语义。
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

    // m_sim 保存内部规则状态；m_state 是映射给 View 的 Common 快照。
    std::unique_ptr<GameSimulation> m_sim;
    GameState m_state;

    // tick 推进规则后同步快照；输入命令只改模拟器，再统一触发通知。
    void tick(float dt);
    void sync_state_from_simulation();
    std::function<void(bool)> make_move_command(MoveInput input);
    std::function<void()> make_action_command(PlayerActionType action);
    void reset_game();
    bool is_gameplay_active() const noexcept;
    void notify_state_changed();
};

} // namespace alleyfist::viewmodel
