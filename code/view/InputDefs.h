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

enum class MovementDirection : std::size_t {
    Left,
    Right,
    Up,
    Down,
    Count
};

inline constexpr std::size_t kMovementDirectionCount =
    static_cast<std::size_t>(MovementDirection::Count);

/// Tracks physical keys separately from the semantic direction they control.
/// This keeps aliases such as A and Left active until both keys are released.
class InputState {
public:
    bool press_movement(MovementDirection direction, int physicalKey)
    {
        const auto directionIndex = index(direction);
        if (directionIndex >= m_movementKeys.size()) return false;
        auto& keys = m_movementKeys[directionIndex];
        if (!keys.insert(physicalKey).second) return false;

        return keys.size() == 1;
    }

    bool release_movement(MovementDirection direction, int physicalKey)
    {
        const auto directionIndex = index(direction);
        if (directionIndex >= m_movementKeys.size()) return false;
        auto& keys = m_movementKeys[directionIndex];
        if (keys.erase(physicalKey) == 0 || !keys.empty()) return false;

        return true;
    }

    bool press_action(int physicalKey)
    {
        return m_actionKeys.insert(physicalKey).second;
    }

    void release_action(int physicalKey)
    {
        m_actionKeys.erase(physicalKey);
    }

    std::array<bool, kMovementDirectionCount> clear_movement()
    {
        std::array<bool, kMovementDirectionCount> active{};
        for (std::size_t i = 0; i < m_movementKeys.size(); ++i) {
            active[i] = !m_movementKeys[i].empty();
            m_movementKeys[i].clear();
        }
        return active;
    }

    void clear_actions()
    {
        m_actionKeys.clear();
    }

private:
    static constexpr std::size_t index(MovementDirection direction) noexcept
    {
        return static_cast<std::size_t>(direction);
    }

    std::array<std::unordered_set<int>, kMovementDirectionCount> m_movementKeys;
    std::unordered_set<int> m_actionKeys;
};

} // namespace view
} // namespace alleyfist
