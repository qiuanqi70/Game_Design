#pragma once

#include <array>
#include <cstddef>
#include <unordered_set>

namespace alleyfist {
namespace view {

// ============================================================================
// View 层内部的输入聚合类型
//
// 输入命令由 App 从 ViewModel 注入。InputState 按语义方向分别记录物理键，
// 换手柄/触屏只需要改 GameWidget 里的按键映射。
// ============================================================================

enum class MovementDirection : std::size_t { Left, Right, Up, Down, Count };

inline constexpr std::size_t kMovementDirectionCount = static_cast<std::size_t>(MovementDirection::Count);

/// Tracks physical keys separately from the semantic direction they control.
/// This keeps aliases such as A and Left active until both keys are released.
class InputState {
public:
    /// 记录一次方向键按下。返回 true 表示该方向从未按下变为按下。
    bool press_movement(MovementDirection direction, int physicalKey) {
        const auto directionIndex = index(direction);
        if (directionIndex >= m_movementKeys.size())
            return false;
        auto& keys = m_movementKeys[directionIndex];
        if (!keys.insert(physicalKey).second)
            return false;

        return keys.size() == 1;
    }

    /// 记录一次方向键释放。返回 true 表示该方向已经没有任何物理键保持按下。
    bool release_movement(MovementDirection direction, int physicalKey) {
        const auto directionIndex = index(direction);
        if (directionIndex >= m_movementKeys.size())
            return false;
        auto& keys = m_movementKeys[directionIndex];
        if (keys.erase(physicalKey) == 0 || !keys.empty())
            return false;

        return true;
    }

    /// 动作键只需要防止自动重复触发，释放时不向 ViewModel 发送持续状态。
    bool press_action(int physicalKey) { return m_actionKeys.insert(physicalKey).second; }

    void release_action(int physicalKey) { m_actionKeys.erase(physicalKey); }

    std::array<bool, kMovementDirectionCount> clear_movement() {
        std::array<bool, kMovementDirectionCount> active{};
        for (std::size_t i = 0; i < m_movementKeys.size(); ++i) {
            active[i] = !m_movementKeys[i].empty();
            m_movementKeys[i].clear();
        }
        return active;
    }

    void clear_actions() { m_actionKeys.clear(); }

private:
    static constexpr std::size_t index(MovementDirection direction) noexcept {
        return static_cast<std::size_t>(direction);
    }

    std::array<std::unordered_set<int>, kMovementDirectionCount> m_movementKeys;
    std::unordered_set<int> m_actionKeys;
};

} // namespace view
} // namespace alleyfist
