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
    Player,
    Patroller,
    Ambusher,
    Charger,
    Ranged,
    Boss
};

enum class ActorActionState {
    Idle,
    Walk,
    LightAttack,
    HeavyAttack,
    RangedAttack,
    Ambush,
    Charge,
    Jump,
    AirAttack,
    Hurt,
    Dead
};

enum class ImpactLevel {
    None,
    Light,
    Heavy
};

enum class Facing {
    Left,
    Right
};

struct ActorState {
    std::uint32_t id = 0;
    ActorKind kind = ActorKind::Player;
    Team team = Team::Player;
    alleyfist::WorldPosition position;
    alleyfist::Size drawSize;
    alleyfist::ResourceBar health;
    ActorActionState actionState = ActorActionState::Idle;
    Facing facing = Facing::Right;
    bool visible = true;
    std::uint32_t impactRevision = 0;
    ImpactLevel lastImpact = ImpactLevel::None;
};

enum class ProjectileKind {
    ThrownObject
};

struct ProjectileState {
    std::uint32_t id = 0;
    ProjectileKind kind = ProjectileKind::ThrownObject;
    Team team = Team::Enemy;
    alleyfist::WorldPosition position;
    Facing facing = Facing::Left;
};

enum class PickupKind {
    Health,
    Energy
};

struct PickupState {
    std::uint32_t id = 0;
    PickupKind kind = PickupKind::Health;
    alleyfist::WorldPosition position;
};

enum class EncounterKind {
    None,
    EnemyWave,
    Boss
};

enum class EncounterPhase {
    None,
    Intro,
    Fighting,
    Cleared
};

struct EncounterState {
    EncounterKind kind = EncounterKind::None;
    EncounterPhase phase = EncounterPhase::None;
    std::uint32_t currentWave = 0;
    std::uint32_t totalWaves = 0;
    std::uint32_t remainingEnemies = 0;
    float introProgress = 0.0f;
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
    std::vector<ProjectileState> projectiles;
    std::vector<PickupState> pickups;
    HudState hud;
    GameResultState result;
    std::string screenMessage;
    EncounterState encounter;
};

} // namespace alleyfist
