#pragma once

#include "types.h"

#include <cstdint>

namespace alleyfist {

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

struct GameResultViewData {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

} // namespace alleyfist
