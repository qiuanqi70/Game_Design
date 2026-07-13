#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

enum class GamePhase {
    Title,
    Playing,
    Paused,
    GameOver,
    Win
};

struct GameResultState {
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

struct MapState {
    float viewportWidth = 960.0f;
    float viewportHeight = 540.0f;
    float cameraX = 0.0f;
    float streetTopY = 300.0f;
    float streetBottomY = 500.0f;
};

enum class Team {
    Player,
    Enemy
};

enum class ActorKind {
    Grunt,
    Boss
};

enum class ActorActionState {
    Idle,
    Walk,
    LightAttack,
    HeavyAttack,
    Jump,
    AirAttack,
    Dead
};

enum class Facing {
    Left,
    Right
};

struct ActorState {
    ActorKind kind = ActorKind::Grunt;
    Team team = Team::Player;
    alleyfist::WorldPosition position;
    alleyfist::Size drawSize;
    alleyfist::ResourceBar health;
    ActorActionState actionState = ActorActionState::Idle;
    Facing facing = Facing::Right;
    bool visible = true;
};

struct HudState {
    alleyfist::ResourceBar playerHealth;
    alleyfist::ResourceBar playerEnergy;
    alleyfist::ResourceBar bossHealth;
    bool showBossHealth = false;
    bool playerExhausted = false;
};

struct GameState {
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    float progressRatio = 0.0f;
    MapState map;
    ActorState player;
    std::vector<ActorState> enemies;
    HudState hud;
    GameResultState result;
    std::string screenMessage;
};

} // namespace alleyfist
