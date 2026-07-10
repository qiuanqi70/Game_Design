#pragma once

#include <cstdint>

namespace alleyfist {

// Common 层的命令只表达“View 想让游戏做什么”，不包含键盘码、Qt 事件或具体规则。
// View 负责把按键翻译成 InputAction，ViewModel 负责解释这些动作会产生什么游戏效果。

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
    Tick
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

};

} // namespace alleyfist
