#pragma once

#include "actor.h"
#include "game_status.h"
#include "types.h"

#include <cstdint>
#include <vector>

namespace alleyfist {

enum class SpawnSide {
    Left,
    Right,
    Both,
    BossGate
};

struct SpawnSpec {
    ActorKind actorKind = ActorKind::Grunt;
    EnemyBehavior behavior = EnemyBehavior::Surround;
    SpawnSide side = SpawnSide::Both;
    std::uint32_t count = 0;
    int maxHealth = 1;
};

struct EncounterSpec {
    EncounterId id = kInvalidEncounterId;
    float triggerX = 0.0f;
    bool bossEncounter = false;
    std::vector<SpawnSpec> spawns;
};

struct EncounterViewData {
    EncounterId id = kInvalidEncounterId;
    EncounterState state = EncounterState::Inactive;
    ScrollLockState lockState = ScrollLockState::Free;
    float triggerX = 0.0f;
    std::uint32_t spawnedCount = 0;
    std::uint32_t defeatedCount = 0;
    std::uint32_t remainingCount = 0;
    bool bossEncounter = false;
};

struct MapViewData {
    float worldWidth = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float cameraX = 0.0f;
    float streetTopY = 0.0f;
    float streetBottomY = 0.0f;
    float leftBoundaryX = 0.0f;
    float rightBoundaryX = 0.0f;
    ScrollLockState scrollLock = ScrollLockState::Free;
    bool showGoIndicator = false;
    std::vector<EncounterViewData> encounters;
};

struct LevelProgressViewData {
    std::uint32_t stageIndex = 0;
    EncounterId activeEncounterId = kInvalidEncounterId;
    float progressRatio = 0.0f;
    bool bossSpawned = false;
    bool bossDefeated = false;
};

} // namespace alleyfist
