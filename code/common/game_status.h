#pragma once

#include "types.h"

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

// 默认规则参数。真正的逻辑由 ViewModel 控制，也可以覆盖这些值。
struct GameRules {
    float levelLength = 3000.0f;
    float viewportWidth = 960.0f;
    float viewportHeight = 540.0f;
    float streetTopY = 300.0f;
    float streetBottomY = 500.0f;
    float playerMoveSpeed = 260.0f;
    float enemyMoveSpeed = 140.0f;
    float bossMoveSpeed = 180.0f;
    float jumpSeconds = 0.55f;
    float jumpHeight = 90.0f;
    float energyRecoveryPerSecond = 18.0f;
    float runEnergyCostPerSecond = 8.0f;
    int lightAttackEnergyCost = 8;
    int heavyAttackEnergyCost = 18;
    int jumpEnergyCost = 12;
};

// 结算界面或 Game Over 界面需要展示的结果数据。
struct GameResultViewData {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

} // namespace alleyfist
