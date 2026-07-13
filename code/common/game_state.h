#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist::viewmodel {

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

struct GameResultViewState {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

struct MapViewState {
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

enum class ActorState {
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

struct ActorViewState {
    ActorKind kind = ActorKind::Prop;
    Team team = Team::Neutral;
    alleyfist::WorldPosition position;
    alleyfist::Size drawSize;
    alleyfist::ResourceBar health;
    alleyfist::ResourceBar energy;
    ActorState state = ActorState::Idle;
    Facing facing = Facing::Right;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
};

struct HudViewState {
    alleyfist::ResourceBar playerHealth;
    alleyfist::ResourceBar playerEnergy;
    alleyfist::ResourceBar bossHealth;
    bool showBossHealth = false;
    std::uint32_t comboStep = 0;
    float comboTimeLeftSeconds = 0.0f;
    bool playerExhausted = false;
};

struct GameViewState {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    float progressRatio = 0.0f;
    MapViewState map;
    ActorViewState player;
    std::vector<ActorViewState> enemies;
    std::vector<ActorViewState> effects;
    HudViewState hud;
    GameResultViewState result;
    std::string screenMessage;
};

} // namespace alleyfist::viewmodel
