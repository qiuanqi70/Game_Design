#pragma once

#include "actor.h"
#include "game_status.h"
#include "level.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

struct HudViewData {
    ResourceBar playerHealth;
    ResourceBar playerEnergy;
    ResourceBar bossHealth;
    bool showBossHealth = false;
    std::uint32_t comboStep = 0;
    float comboTimeLeftSeconds = 0.0f;
    bool playerExhausted = false;
};

struct GameSnapshot {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    MapViewData map;
    LevelProgressViewData progress;
    ActorViewData player;
    std::vector<ActorViewData> enemies;
    std::vector<ActorViewData> effects;
    HudViewData hud;
    GameResultViewData result;
    std::string screenMessage;
};

} // namespace alleyfist
