#pragma once

#include <cstdint>

namespace alleyfist {

// Common 层的命令只表达“View 想让游戏做什么”，不包含键盘码、Qt 事件或具体规则。
// View 负责把按键翻译成 InputAction，ViewModel 负责解释这些动作会产生什么游戏效果。

// Direction 是组合后的移动意图，不是键盘按键码。
// ViewModel 可以根据当前按住的移动键推导出它。
enum class Direction {
    None,
    Left,
    Right,
    Up,
    Down,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight
};

// View 发出的逻辑动作。具体按键绑定留在 View，玩法含义放在这里。
enum class InputAction {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    LightAttack,
    HeavyAttack,
    Jump,
    Restart,
    Confirm,
    Pause
};

// Pressed/Released 用于持续按键；Triggered 用于单次触发动作。
enum class ButtonState {
    Pressed,
    Released,
    Triggered
};

enum class CommandType {
    Input,
    Tick,
    Restart,
    Pause,
    Resume
};

// 记录当前按住的移动键，方便表达斜向移动。
struct MovementIntent {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;

    bool is_moving() const noexcept
    {
        return left || right || up || down;
    }

    Direction direction() const noexcept
    {
        if (left && up && !right && !down) {
            return Direction::UpLeft;
        }
        if (right && up && !left && !down) {
            return Direction::UpRight;
        }
        if (left && down && !right && !up) {
            return Direction::DownLeft;
        }
        if (right && down && !left && !up) {
            return Direction::DownRight;
        }
        if (left && !right) {
            return Direction::Left;
        }
        if (right && !left) {
            return Direction::Right;
        }
        if (up && !down) {
            return Direction::Up;
        }
        if (down && !up) {
            return Direction::Down;
        }
        return Direction::None;
    }
};

struct InputCommand {
    InputAction action = InputAction::Confirm;
    ButtonState state = ButtonState::Triggered;
};

// Tick 驱动和时间有关的逻辑，例如动画、AI、精力恢复和跳跃。
struct TickCommand {
    float deltaSeconds = 0.0f;
    std::uint64_t frameIndex = 0;
};

// View/App 传给 ViewModel 的统一命令包装。
struct GameCommand {
    CommandType type = CommandType::Input;
    InputCommand input;
    TickCommand tick;

    static GameCommand input_command(InputAction action, ButtonState state) noexcept
    {
        GameCommand command;
        command.type = CommandType::Input;
        command.input = {action, state};
        return command;
    }

    static GameCommand tick_command(float deltaSeconds, std::uint64_t frameIndex) noexcept
    {
        GameCommand command;
        command.type = CommandType::Tick;
        command.tick = {deltaSeconds, frameIndex};
        return command;
    }

    static GameCommand simple_command(CommandType type) noexcept
    {
        GameCommand command;
        command.type = type;
        return command;
    }
};

} // namespace alleyfist
