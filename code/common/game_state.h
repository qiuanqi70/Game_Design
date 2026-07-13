#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

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

struct GameResultState {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

struct MapState {
    float worldWidth = 3000.0f;
    float viewportWidth = 960.0f;
    float viewportHeight = 540.0f;
    float cameraX = 0.0f;
    float streetTopY = 300.0f;
    float streetBottomY = 500.0f;
    float leftBoundaryX = 0.0f;
    float rightBoundaryX = 960.0f;
    bool showGoIndicator = false;
};

enum class Team {
    Neutral,
    Player,
    Enemy
};

enum class ActorKind {
    Player,
    Grunt,
    Boss,
    Prop,
    Effect
};

enum class ActorActionState {
    Idle,
    Walk,
    Run,
    LightAttack,
    HeavyAttack,
    ComboFinisher,
    Jump,
    AirAttack,
    Hurt,
    KnockedDown,
    Dead,
    Spawn
};

enum class Facing {
    Left,
    Right
};

struct ActorState {
    ActorKind kind = ActorKind::Prop;
    Team team = Team::Neutral;
    alleyfist::WorldPosition position;
    alleyfist::Size drawSize;
    alleyfist::ResourceBar health;
    alleyfist::ResourceBar energy;
    ActorActionState actionState = ActorActionState::Idle;
    Facing facing = Facing::Right;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
};

struct HudState {
    alleyfist::ResourceBar playerHealth;
    alleyfist::ResourceBar playerEnergy;
    alleyfist::ResourceBar bossHealth;
    bool showBossHealth = false;
    std::uint32_t comboStep = 0;
    float comboTimeLeftSeconds = 0.0f;
    bool playerExhausted = false;
};

struct GameState {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    float progressRatio = 0.0f;
    MapState map;
    ActorState player;
    std::vector<ActorState> enemies;
    std::vector<ActorState> effects;
    HudState hud;
    GameResultState result;
    std::string screenMessage;
};

} // namespace alleyfist
