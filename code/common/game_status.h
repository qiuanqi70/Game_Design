#pragma once

#include <cstdint>

namespace alleyfist {

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
