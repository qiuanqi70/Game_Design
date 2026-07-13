#pragma once

namespace alleyfist {
namespace view {

// ============================================================================
// View 层内部的输入聚合类型
//
// InputAction / ButtonState 已经移到 Common 合约之外（它们属于输入设备层）。
// 这里只保留 MovementIntent，用于把多个按键状态聚合成一个移动意图。
// 换手柄/触屏只需要改 GameWidget 里的按键映射，不影响这里的聚合逻辑。
// ============================================================================

// 记录当前按住的移动键，用于推导复合移动方向。
struct MovementIntent {
    bool left  = false;
    bool right = false;
    bool up    = false;
    bool down  = false;

    bool is_moving() const noexcept
    {
        return left || right || up || down;
    }

    void clear() noexcept
    {
        left  = false;
        right = false;
        up    = false;
        down  = false;
    }
};

} // namespace view
} // namespace alleyfist
