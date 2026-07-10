#pragma once

#include <cstdint>

namespace alleyfist {

// 这里的状态是 View 需要观察的“游戏处于哪个阶段”，不是驱动状态变化的规则实现。
// 规则参数和判定逻辑属于 ViewModel 层，Common 只保留跨层展示契约。

// 游戏主流程状态。View 可以根据它切换界面或覆盖层。
enum class GamePhase {
    Title,
    Playing,
    EncounterLocked,
    ClearToGo,
    Paused,
    GameOver,
    Win
};

enum class GameOverReason {
    None,
    PlayerDefeated
};

enum class WinReason {
    None,
    BossDefeated
};

enum class EncounterState {
    Inactive,
    Triggered,
    Locked,
    Cleared
};

enum class ScrollLockState {
    Free,
    LockedByEncounter,
    LockedByBoss,
    LevelFinished
};

// 结算界面或 Game Over 界面需要展示的结果数据。
struct GameResultViewData {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

} // namespace alleyfist
