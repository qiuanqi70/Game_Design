#pragma once

#include <cstdint>

namespace alleyfist {

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

struct TickCommand {
    float deltaSeconds = 0.0f;
    std::uint64_t frameIndex = 0;
};

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
