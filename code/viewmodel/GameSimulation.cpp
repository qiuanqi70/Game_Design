#include "GameSimulation.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace alleyfist::viewmodel {

namespace {

constexpr float kBossTriggerX = 2350.0f;
constexpr float kBossSpawnOffsetX = 260.0f;
constexpr float kBossArenaLeft = 2180.0f;
constexpr float kBossArenaRight = 2920.0f;
constexpr int kBossHealth = 140;
constexpr float kEncounterIntroSeconds = 2.8f;
constexpr float kWaveTransitionSeconds = 1.0f;
constexpr std::uint32_t kWaveCount = 3;
constexpr std::array<float, kWaveCount> kWaveRightGateX = {
    760.0f, 1540.0f, 2260.0f
};
constexpr float kMeleeAttackDuration = 0.36f;
constexpr float kMeleeAttackHitTime = 0.17f;
constexpr float kMeleeAttackCooldown = 0.9f;
constexpr float kAttackWindup = 0.14f;
constexpr float kAmbushDuration = 0.65f;
constexpr float kRangedAttackCooldown = 1.8f;
constexpr float kRangedAttackDuration = 0.32f;
constexpr float kChargeDuration = 0.55f;
constexpr float kChargeWindup = 0.18f;
constexpr float kBossChargeCooldown = 1.1f;
constexpr float kHurtDuration = 0.25f;
constexpr float kEnemyDeathDisplaySeconds = 0.75f;
constexpr float kProjectileSpeed = 260.0f;
constexpr float kProjectileGravity = 80.0f;
constexpr float kPickupLifetime = 10.0f;
constexpr float kContactRangeX = 18.0f;
constexpr float kContactRangeY = 16.0f;
constexpr float kContactRangeZ = 36.0f;
constexpr float kMoveSpeed = 220.0f;
constexpr float kJumpSeconds = 0.55f;
constexpr float kJumpHeight = 90.0f;
constexpr float kJumpEnergyCost = 18.0f;
constexpr float kEnergyRegenPerSecond = 28.0f;
constexpr float kExhaustedWarningSeconds = 0.65f;
constexpr float kPi = 3.1415926535f;

struct PlayerAttackSpec {
    float duration;
    float hitTime;
    float energyCost;
    int damage;
    ImpactLevel impact;
    float rangeX;
    float rangeY;
};

PlayerAttackSpec player_attack_spec(PlayerActionType action)
{
    if (action == PlayerActionType::HeavyAttack) {
        return {0.30f, 0.18f, 25.0f, 18, ImpactLevel::Heavy, 72.0f, 44.0f};
    }
    return {0.18f, 0.08f, 12.0f, 10, ImpactLevel::Light, 55.0f, 35.0f};
}

bool overlaps(const EntityState& first, const EntityState& second,
              float rangeX, float rangeY, float rangeZ = kContactRangeZ)
{
    return std::abs(first.pos.x - second.pos.x) < rangeX &&
           std::abs(first.pos.laneY - second.pos.laneY) < rangeY &&
           std::abs(first.pos.z - second.pos.z) < rangeZ;
}

EnemyBehaviorState default_behavior_for(EntityKind kind)
{
    switch (kind) {
    case EntityKind::Patroller:
        return EnemyBehaviorState::Patrol;
    case EntityKind::Boss:
        return EnemyBehaviorState::Patrol;
    case EntityKind::Player:
    case EntityKind::Ambusher:
    case EntityKind::Charger:
    case EntityKind::Ranged:
        return EnemyBehaviorState::Idle;
    }
    return EnemyBehaviorState::Idle;
}

} // namespace

GameSimulation::GameSimulation()
{
    populate_initial_state();
}

void GameSimulation::step(float deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }

    m_elapsedSeconds += deltaSeconds;
    update_player_state(deltaSeconds);
    move_player(deltaSeconds);
    update_wave_progression(deltaSeconds);
    update_encounter_state(deltaSeconds);
    spawn_boss_if_needed();
    simulate_ai(deltaSeconds);
    update_death_presentations(deltaSeconds);
    update_projectiles(deltaSeconds);
    update_pickups(deltaSeconds);
    update_boss_state();
    if (m_bossDefeated) {
        update_encounter_state(0.0f);
    } else if (m_bossSpawned && m_encounterTimer >= kEncounterIntroSeconds) {
        m_encounter.phase = EncounterPhase::Fighting;
        m_encounter.introProgress = 1.0f;
    }
}

void GameSimulation::populate_initial_state()
{
    m_entities.clear();
    m_projectiles.clear();
    m_pickups.clear();
    m_bossSpawned = false;
    m_bossDefeated = false;
    m_bossVictoryReady = false;
    m_bossIntroStarted = false;
    clear_movement_input();
    m_jumpActive = false;
    m_nextId = 1;
    m_nextProjectileId = 1;
    m_nextPickupId = 1;
    m_elapsedSeconds = 0.0f;
    m_encounterTimer = 0.0f;
    m_playerEnergy = kMaxPlayerEnergy;
    m_jumpElapsed = 0.0f;
    m_attackTimer = 0.0f;
    m_attackElapsed = 0.0f;
    m_exhaustedWarningTimer = 0.0f;
    m_encounter = {};
    m_currentWave = 0;
    m_wavePhase = WavePhase::Fighting;
    m_waveTransitionTimer = 0.0f;
    m_activePlayerAction = PlayerActionType::None;
    m_attackHitApplied = false;

    EntityState player;
    player.id = m_nextId++;
    player.kind = EntityKind::Player;
    player.hp = 100;
    player.maxHp = 100;
    player.pos.x = 80.0f;
    player.pos.laneY = 400.0f;
    player.facing = Facing::Right;
    player.behaviorState = EnemyBehaviorState::Idle;
    player.alive = true;
    m_entities.push_back(player);

    spawn_wave_enemies(0);
}

void GameSimulation::update_player_state(float dt) noexcept
{
    if (m_entities.empty()) {
        return;
    }

    auto& player = m_entities.front();
    m_exhaustedWarningTimer = std::max(0.0f, m_exhaustedWarningTimer - dt);
    if (player.alive) {
        m_playerEnergy = std::clamp(
            m_playerEnergy + kEnergyRegenPerSecond * dt, 0.0f, kMaxPlayerEnergy);
    }

    if (player.hurtTimer > 0.0f) {
        player.hurtTimer = std::max(0.0f, player.hurtTimer - dt);
        if (player.hurtTimer <= 0.0f) {
            player.behaviorState = EnemyBehaviorState::Idle;
        }
    }

    if (m_jumpActive) {
        m_jumpElapsed += dt;
        if (m_jumpElapsed >= kJumpSeconds) {
            m_jumpActive = false;
            m_jumpElapsed = 0.0f;
            player.pos.z = 0.0f;
        } else {
            const float progress = std::clamp(m_jumpElapsed / kJumpSeconds, 0.0f, 1.0f);
            player.pos.z = kJumpHeight * std::sin(kPi * progress);
        }
    } else {
        player.pos.z = 0.0f;
    }

    if (m_attackTimer > 0.0f) {
        m_attackElapsed += dt;
        if (!m_attackHitApplied) {
            const auto spec = player_attack_spec(m_activePlayerAction);
            if (m_attackElapsed >= spec.hitTime) {
                resolve_player_attack(m_activePlayerAction);
                m_attackHitApplied = true;
            }
        }
        m_attackTimer = std::max(0.0f, m_attackTimer - dt);
        if (m_attackTimer <= 0.0f) {
            m_activePlayerAction = PlayerActionType::None;
            m_attackElapsed = 0.0f;
            m_attackHitApplied = false;
        }
    }
}

void GameSimulation::move_player(float dt) noexcept
{
    if (m_entities.empty()) {
        return;
    }

    auto& player = m_entities.front();
    if (!player.alive || player.hurtTimer > 0.0f || m_attackTimer > 0.0f) {
        return;
    }

    const float dx = (m_moveRight ? kMoveSpeed : 0.0f) -
                     (m_moveLeft ? kMoveSpeed : 0.0f);
    const float dy = (m_moveDown ? kMoveSpeed : 0.0f) -
                     (m_moveUp ? kMoveSpeed : 0.0f);
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }

    const auto bounds = player_movement_bounds();
    player.pos.x = std::clamp(player.pos.x + dx * dt,
                              bounds.minimumX, bounds.maximumX);
    player.pos.laneY = std::clamp(player.pos.laneY + dy * dt,
                                  kStreetTop, kStreetBottom);
    if (dx < 0.0f) {
        player.facing = Facing::Left;
    } else if (dx > 0.0f) {
        player.facing = Facing::Right;
    }
}

bool GameSimulation::try_spend_energy(float cost) noexcept
{
    if (m_playerEnergy < cost) {
        m_exhaustedWarningTimer = kExhaustedWarningSeconds;
        return false;
    }

    m_playerEnergy = std::clamp(m_playerEnergy - cost, 0.0f, kMaxPlayerEnergy);
    m_exhaustedWarningTimer = 0.0f;
    return true;
}

void GameSimulation::clear_movement_input() noexcept
{
    m_moveLeft = false;
    m_moveRight = false;
    m_moveUp = false;
    m_moveDown = false;
}

bool GameSimulation::request_player_action(PlayerActionType action) noexcept
{
    if (m_entities.empty() || action == PlayerActionType::None) {
        return false;
    }

    auto& player = m_entities.front();
    if (!player.alive || player.hurtTimer > 0.0f || m_attackTimer > 0.0f) {
        return false;
    }

    if (action == PlayerActionType::Jump) {
        if (m_jumpActive || !try_spend_energy(kJumpEnergyCost)) {
            return false;
        }
        m_jumpActive = true;
        m_jumpElapsed = 0.0f;
        return true;
    }

    if (action != PlayerActionType::LightAttack &&
        action != PlayerActionType::HeavyAttack) {
        return false;
    }

    const auto spec = player_attack_spec(action);
    if (!try_spend_energy(spec.energyCost)) {
        return false;
    }

    m_activePlayerAction = action;
    m_attackTimer = spec.duration;
    m_attackElapsed = 0.0f;
    m_attackHitApplied = false;
    return true;
}

PlayerBehaviorState GameSimulation::player_behavior_state() const noexcept
{
    if (m_entities.empty() || !m_entities.front().alive) {
        return PlayerBehaviorState::Dead;
    }
    if (m_entities.front().hurtTimer > 0.0f) {
        return PlayerBehaviorState::Hurt;
    }
    if (m_attackTimer > 0.0f) {
        if (m_jumpActive) {
            return PlayerBehaviorState::AirAttack;
        }
        return m_activePlayerAction == PlayerActionType::HeavyAttack
                   ? PlayerBehaviorState::HeavyAttack
                   : PlayerBehaviorState::LightAttack;
    }
    if (m_jumpActive) {
        return PlayerBehaviorState::Jump;
    }
    if ((m_moveLeft != m_moveRight) || (m_moveUp != m_moveDown)) {
        return PlayerBehaviorState::Walk;
    }
    return PlayerBehaviorState::Idle;
}

void GameSimulation::resolve_player_attack(PlayerActionType action) noexcept
{
    if (m_entities.size() < 2) {
        return;
    }

    auto& player = m_entities.front();
    const auto spec = player_attack_spec(action);
    const float rangeZ = m_jumpActive ? kJumpHeight + kContactRangeZ
                                      : kContactRangeZ;

    for (std::size_t i = 1; i < m_entities.size(); ++i) {
        auto& enemy = m_entities[i];
        if (!enemy.alive) {
            continue;
        }
        const float bossRangeBonusX = enemy.kind == EntityKind::Boss ? 30.0f : 0.0f;
        const float bossRangeBonusY = enemy.kind == EntityKind::Boss ? 10.0f : 0.0f;
        if (overlaps(enemy, player, spec.rangeX + bossRangeBonusX,
                     spec.rangeY + bossRangeBonusY, rangeZ)) {
            apply_damage(enemy, spec.damage, spec.impact);
            break;
        }
    }

    update_boss_state();
    update_wave_progression(0.0f);
    update_encounter_state(0.0f);
}

void GameSimulation::spawn_wave_enemies(std::uint32_t waveIndex)
{
    if (waveIndex >= kWaveCount) return;

    constexpr std::array<std::array<EntityKind, 4>, 3> kWaveEnemies = {{
        {EntityKind::Patroller, EntityKind::Patroller, EntityKind::Patroller, EntityKind::Patroller},
        {EntityKind::Ambusher, EntityKind::Charger, EntityKind::Ranged, EntityKind::Patroller},
        {EntityKind::Ambusher, EntityKind::Charger, EntityKind::Ranged, EntityKind::Ambusher}
    }};

    constexpr std::array<std::array<float, 4>, 3> kWaveEnemyX = {{
        {320.0f, 420.0f, 520.0f, 620.0f},
        {1100.0f, 1200.0f, 1300.0f, 1400.0f},
        {1880.0f, 1980.0f, 2080.0f, 2180.0f}
    }};

    constexpr std::array<std::array<float, 4>, 3> kWaveEnemyY = {{
        {380.0f, 420.0f, 390.0f, 430.0f},
        {370.0f, 430.0f, 400.0f, 420.0f},
        {380.0f, 420.0f, 390.0f, 430.0f}
    }};

    const auto& waveKinds = kWaveEnemies[waveIndex];
    const auto& waveX = kWaveEnemyX[waveIndex];
    const auto& waveY = kWaveEnemyY[waveIndex];

    for (std::size_t i = 0; i < waveKinds.size(); ++i) {
        EntityState e;
        e.id = m_nextId++;
        e.kind = waveKinds[i];
        if (e.kind == EntityKind::Ranged) {
            e.visualVariant = waveIndex == 1 ? ActorVisualVariant::RangedGunner
                                             : ActorVisualVariant::RangedRobot;
        }
        e.behaviorState = (e.kind == EntityKind::Patroller) ? EnemyBehaviorState::Patrol : EnemyBehaviorState::Idle;
        e.hp = 20 + waveIndex * 5;
        e.maxHp = e.hp;
        e.pos.x = waveX[i];
        e.pos.laneY = waveY[i];
        e.patrolRangeLeft = e.pos.x - 110.0f;
        e.patrolRangeRight = e.pos.x + 110.0f;
        e.alive = true;
        m_entities.push_back(e);
    }
}

std::uint32_t GameSimulation::alive_regular_enemy_count() const noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        m_entities.begin(), m_entities.end(), [](const EntityState& entity) {
            return entity.alive && entity.kind != EntityKind::Player &&
                   entity.kind != EntityKind::Boss;
        }));
}

bool GameSimulation::regular_waves_cleared() const noexcept
{
    return m_wavePhase == WavePhase::Complete;
}

GameSimulation::MovementBounds GameSimulation::player_movement_bounds() const noexcept
{
    if (m_bossSpawned || m_bossDefeated) {
        return {kBossArenaLeft, kBossArenaRight};
    }
    if (m_bossIntroStarted) {
        return {kBossArenaLeft, kBossTriggerX};
    }
    if (regular_waves_cleared()) {
        return {0.0f, kBossTriggerX};
    }

    const auto waveIndex = std::min<std::size_t>(m_currentWave,
                                                 kWaveRightGateX.size() - 1u);
    return {0.0f, kWaveRightGateX[waveIndex]};
}

void GameSimulation::update_wave_progression(float dt)
{
    if (m_bossSpawned || m_bossDefeated || m_bossIntroStarted ||
        m_wavePhase == WavePhase::Complete || m_entities.empty()) {
        return;
    }

    const auto& player = m_entities.front();
    if (!player.alive) {
        return;
    }

    if (m_wavePhase == WavePhase::Fighting) {
        if (alive_regular_enemy_count() > 0) {
            return;
        }
        m_wavePhase = WavePhase::Transition;
        m_waveTransitionTimer = 0.0f;
    }

    if (m_wavePhase != WavePhase::Transition) {
        return;
    }

    m_waveTransitionTimer += std::max(0.0f, dt);
    if (m_waveTransitionTimer < kWaveTransitionSeconds) {
        return;
    }

    m_waveTransitionTimer = 0.0f;
    if (m_currentWave + 1u < kWaveCount) {
        ++m_currentWave;
        spawn_wave_enemies(m_currentWave);
        m_wavePhase = WavePhase::Fighting;
    } else {
        m_wavePhase = WavePhase::Complete;
    }
}

void GameSimulation::update_encounter_state(float dt)
{
    if (m_bossDefeated) {
        m_encounter.kind = m_bossSpawned ? EncounterKind::Boss : EncounterKind::EnemyWave;
        m_encounter.phase = EncounterPhase::Cleared;
        m_encounter.currentWave = 0;
        m_encounter.totalWaves = 0;
        m_encounter.remainingEnemies = 0;
        m_encounter.introProgress = 1.0f;
        return;
    }

    if (m_bossSpawned) {
        const bool introCompleted = m_encounterTimer >= kEncounterIntroSeconds;
        m_encounter.kind = EncounterKind::Boss;
        m_encounter.phase = introCompleted ? EncounterPhase::Fighting : EncounterPhase::Intro;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = 1;
        m_encounter.introProgress = std::clamp(m_encounterTimer / kEncounterIntroSeconds, 0.0f, 1.0f);
        if (introCompleted) {
            m_encounterTimer = kEncounterIntroSeconds;
        }
        return;
    }

    if (m_entities.empty()) {
        return;
    }

    const std::uint32_t remainingEnemies = alive_regular_enemy_count();

    const auto& player = m_entities.front();
    if (!m_bossIntroStarted && regular_waves_cleared() && player.alive &&
        player.pos.x >= kBossTriggerX) {
        m_bossIntroStarted = true;
        m_encounterTimer = 0.0f;
    }

    if (m_bossIntroStarted) {
        m_encounterTimer = std::min(kEncounterIntroSeconds,
                                    m_encounterTimer + std::max(0.0f, dt));
        m_encounter.kind = EncounterKind::Boss;
        m_encounter.phase = EncounterPhase::Intro;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = remainingEnemies;
        m_encounter.introProgress = std::clamp(m_encounterTimer / kEncounterIntroSeconds, 0.0f, 1.0f);
        return;
    }

    m_encounter.kind = EncounterKind::EnemyWave;
    m_encounter.totalWaves = kWaveCount;
    m_encounter.remainingEnemies = remainingEnemies;

    if (m_wavePhase == WavePhase::Transition) {
        m_encounter.phase = EncounterPhase::Intro;
        m_encounter.currentWave = std::min(m_currentWave + 2u, kWaveCount);
        m_encounter.introProgress = std::clamp(
            m_waveTransitionTimer / kWaveTransitionSeconds, 0.0f, 1.0f);
    } else if (m_wavePhase == WavePhase::Complete) {
        m_encounter.phase = EncounterPhase::Cleared;
        m_encounter.currentWave = kWaveCount;
        m_encounter.introProgress = 1.0f;
    } else {
        m_encounter.kind = EncounterKind::EnemyWave;
        m_encounter.phase = EncounterPhase::Fighting;
        m_encounter.currentWave = m_currentWave + 1;
        m_encounter.introProgress = 1.0f;
    }
}

void GameSimulation::simulate_ai(float dt)
{
    if (m_entities.empty()) {
        return;
    }

    auto& player = m_entities.front();
    if (player.alive) {
        for (std::size_t i = 1; i < m_entities.size(); ++i) {
            auto& enemy = m_entities[i];
            if (!enemy.alive) {
                continue;
            }

            if (enemy.hurtTimer > 0.0f) {
                enemy.hurtTimer = std::max(0.0f, enemy.hurtTimer - dt);
                if (enemy.hurtTimer > 0.0f) {
                    continue;
                }
                enemy.behaviorState = default_behavior_for(enemy.kind);
            }

            enemy.ambushCooldown = std::max(0.0f, enemy.ambushCooldown - dt);
            enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);

            const float dx = player.pos.x - enemy.pos.x;
            if (std::abs(dx) > 0.001f) {
                enemy.facing = dx > 0.0f ? Facing::Right : Facing::Left;
            }

            switch (enemy.kind) {
            case EntityKind::Patroller:
                update_patroller(enemy, player, dt);
                break;
            case EntityKind::Ambusher:
                update_ambusher(enemy, player, dt);
                break;
            case EntityKind::Charger:
                update_charger(enemy, player, dt);
                break;
            case EntityKind::Ranged:
                update_ranged_enemy(enemy, player, dt);
                break;
            case EntityKind::Boss:
                update_boss(enemy, player, dt);
                break;
            case EntityKind::Player:
                break;
            }
        }
    }

    process_enemy_deaths();
}

void GameSimulation::update_patroller(EntityState& enemy, EntityState& player, float dt)
{
    if (enemy.behaviorState == EnemyBehaviorState::MeleeAttack) {
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
        if (!enemy.attackHitApplied && enemy.attackTimer <= kMeleeAttackHitTime) {
            enemy.attackHitApplied = true;
            if (overlaps(enemy, player, kContactRangeX, kContactRangeY)) {
                apply_damage(player, 1, ImpactLevel::Light);
            }
        }
        if (enemy.attackTimer <= 0.0f) {
            enemy.behaviorState = EnemyBehaviorState::Patrol;
        }
        return;
    }

    const float moveDirection =
        enemy.pos.x < enemy.patrolRangeLeft + 5.0f ? 1.0f
        : enemy.pos.x > enemy.patrolRangeRight - 5.0f ? -1.0f
                                                            : 0.0f;
    const float patrolDirection = moveDirection != 0.0f
                                      ? moveDirection
                                      : (std::sin(m_elapsedSeconds * 0.9f) >= 0.0f
                                             ? 1.0f
                                             : -1.0f);
    enemy.pos.x += 18.0f * dt * patrolDirection;

    const float dx = player.pos.x - enemy.pos.x;
    const float dy = player.pos.laneY - enemy.pos.laneY;
    if (enemy.attackCooldown <= 0.0f &&
        std::abs(dx) < 34.0f && std::abs(dy) < 24.0f) {
        enemy.behaviorState = EnemyBehaviorState::MeleeAttack;
        enemy.attackTimer = kMeleeAttackDuration;
        enemy.attackCooldown = kMeleeAttackCooldown;
        enemy.attackHitApplied = false;
    }
}

void GameSimulation::update_ambusher(EntityState& enemy, EntityState& player, float dt)
{
    const float dx = player.pos.x - enemy.pos.x;
    const float dy = player.pos.laneY - enemy.pos.laneY;
    const float distance = std::sqrt(dx * dx + dy * dy) + 0.0001f;

    if (enemy.behaviorState == EnemyBehaviorState::Ambush) {
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
        if (enemy.attackTimer <= kAmbushDuration - kAttackWindup &&
            enemy.attackTimer > 0.0f) {
            enemy.pos.x += (dx / distance) * 90.0f * dt;
            enemy.pos.laneY += (dy / distance) * 90.0f * dt;
            hit_player_once(enemy, player, 1, ImpactLevel::Light);
        }
        if (enemy.attackTimer <= 0.0f) {
            enemy.behaviorState = EnemyBehaviorState::Idle;
        }
        return;
    }

    if (std::abs(dx) < 220.0f && std::abs(dy) < 120.0f &&
        enemy.ambushCooldown <= 0.0f) {
        enemy.behaviorState = EnemyBehaviorState::Ambush;
        enemy.ambushCooldown = 1.8f;
        enemy.attackTimer = kAmbushDuration;
        enemy.attackHitApplied = false;
    }
}

void GameSimulation::update_charger(EntityState& enemy, EntityState& player, float dt)
{
    const float dx = player.pos.x - enemy.pos.x;
    const float dy = player.pos.laneY - enemy.pos.laneY;
    const float distance = std::sqrt(dx * dx + dy * dy) + 0.0001f;

    if (enemy.behaviorState == EnemyBehaviorState::Charge) {
        if (enemy.attackTimer > 0.0f) {
            enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
            return;
        }

        enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);
        if (enemy.chargeTimer > 0.0f) {
            enemy.pos.x += (dx / distance) * 220.0f * dt;
            enemy.pos.laneY += (dy / distance) * 220.0f * dt;
            hit_player_once(enemy, player, 1, ImpactLevel::Heavy);
        } else {
            enemy.behaviorState = EnemyBehaviorState::Idle;
        }
        return;
    }

    if (std::abs(dx) < 180.0f && std::abs(dy) < 90.0f &&
        enemy.attackCooldown <= 0.0f) {
        enemy.behaviorState = EnemyBehaviorState::Charge;
        enemy.chargeTimer = kChargeDuration;
        enemy.attackTimer = kChargeWindup;
        enemy.attackCooldown = 1.4f;
        enemy.attackHitApplied = false;
    }
}

void GameSimulation::update_ranged_enemy(EntityState& enemy, EntityState& player, float dt)
{
    const float dx = player.pos.x - enemy.pos.x;
    const float dy = player.pos.laneY - enemy.pos.laneY;
    const float distance = std::sqrt(dx * dx + dy * dy) + 0.0001f;

    if (enemy.behaviorState == EnemyBehaviorState::RangedAttack) {
        enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
        if (!enemy.attackHitApplied &&
            enemy.attackTimer <= kRangedAttackDuration - kAttackWindup) {
            spawn_ranged_projectile(enemy);
            enemy.attackHitApplied = true;
        }
        if (enemy.attackTimer <= 0.0f) {
            enemy.behaviorState = EnemyBehaviorState::Idle;
        }
        return;
    }

    if (enemy.attackCooldown <= 0.0f &&
        std::abs(dx) < 360.0f && std::abs(dy) < 160.0f) {
        enemy.behaviorState = EnemyBehaviorState::RangedAttack;
        enemy.attackCooldown = kRangedAttackCooldown;
        enemy.attackTimer = kRangedAttackDuration;
        enemy.attackHitApplied = false;
        return;
    }
    if (std::abs(dx) > 5.0f) {
        enemy.pos.x += (dx / distance) * 18.0f * dt;
    }
}

void GameSimulation::update_boss(EntityState& enemy, EntityState& player, float dt)
{
    const float dx = player.pos.x - enemy.pos.x;
    const float dy = player.pos.laneY - enemy.pos.laneY;
    const float distance = std::sqrt(dx * dx + dy * dy) + 0.0001f;

    if (enemy.behaviorState == EnemyBehaviorState::Charge) {
        if (enemy.attackTimer > 0.0f) {
            enemy.attackTimer = std::max(0.0f, enemy.attackTimer - dt);
            return;
        }

        enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);
        if (enemy.chargeTimer > 0.0f) {
            enemy.pos.x += (dx / distance) * 180.0f * dt;
            enemy.pos.laneY += (dy / distance) * 180.0f * dt;
            hit_player_once(enemy, player, 2, ImpactLevel::Heavy);
        } else {
            enemy.behaviorState = EnemyBehaviorState::Patrol;
        }
        return;
    }

    if (std::abs(dx) < 150.0f && std::abs(dy) < 90.0f &&
        enemy.attackCooldown <= 0.0f) {
        enemy.behaviorState = EnemyBehaviorState::Charge;
        enemy.chargeTimer = kChargeDuration;
        enemy.attackTimer = kChargeWindup;
        enemy.attackCooldown = kBossChargeCooldown;
        enemy.attackHitApplied = false;
        return;
    }

    enemy.pos.x += (dx / distance) * 40.0f * dt;
    enemy.pos.laneY += (dy / distance) * 40.0f * dt;
}

void GameSimulation::hit_player_once(EntityState& enemy, EntityState& player,
                                     int damage, ImpactLevel impact)
{
    if (enemy.attackHitApplied ||
        !overlaps(enemy, player, kContactRangeX, kContactRangeY)) {
        return;
    }
    enemy.attackHitApplied = true;
    apply_damage(player, damage, impact);
}

void GameSimulation::process_enemy_deaths()
{
    for (auto& enemy : m_entities) {
        if (enemy.kind == EntityKind::Player || enemy.hp > 0 ||
            enemy.pickupDropped) {
            continue;
        }

        if (enemy.alive) {
            enemy.alive = false;
            enemy.deathTimer = kEnemyDeathDisplaySeconds;
        }
        if (enemy.kind == EntityKind::Boss) {
            spawn_pickup(enemy.pos, PickupKind::Health);
            spawn_pickup(enemy.pos, PickupKind::Energy);
        } else {
            const auto pickupKind = enemy.kind == EntityKind::Ranged
                                        ? PickupKind::Energy
                                        : PickupKind::Health;
            spawn_pickup(enemy.pos, pickupKind);
        }
        enemy.pickupDropped = true;
    }
}

void GameSimulation::update_death_presentations(float dt) noexcept
{
    for (auto& entity : m_entities) {
        if (entity.kind == EntityKind::Player || entity.alive || entity.deathTimer <= 0.0f) {
            continue;
        }
        entity.deathTimer = std::max(0.0f, entity.deathTimer - dt);
    }
}

void GameSimulation::update_projectiles(float dt)
{
    for (auto it = m_projectiles.begin(); it != m_projectiles.end();) {
        auto& projectile = *it;
        if (!projectile.active) {
            ++it;
            continue;
        }
        projectile.position.x += projectile.velocityX * dt;
        projectile.position.laneY += projectile.velocityLaneY * dt;
        projectile.position.z += projectile.velocityZ * dt;
        projectile.lifeSeconds -= dt;
        projectile.velocityZ -= kProjectileGravity * dt;

        if (m_entities.empty()) {
            projectile.active = false;
            ++it;
            continue;
        }

        auto& player = m_entities.front();
        if (std::abs(projectile.position.x - player.pos.x) < 30.0f &&
            std::abs(projectile.position.laneY - player.pos.laneY) < 24.0f &&
            std::abs(projectile.position.z - player.pos.z) < kContactRangeZ) {
            apply_damage(player, 8, ImpactLevel::Heavy);
            projectile.active = false;
            ++it;
            continue;
        }

        if (projectile.lifeSeconds <= 0.0f || projectile.position.x < -100.0f || projectile.position.x > kWorldWidth + 200.0f) {
            projectile.active = false;
        }
        ++it;
    }

    m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(), [](const ProjectileVm& projectile) {
        return !projectile.active;
    }), m_projectiles.end());
}

void GameSimulation::update_pickups(float dt)
{
    for (auto& pickup : m_pickups) {
        pickup.lifeSeconds -= dt;
        if (pickup.lifeSeconds <= 0.0f) {
            pickup.active = false;
            continue;
        }

        if (m_entities.empty()) {
            continue;
        }

        auto& player = m_entities.front();
        if (!player.alive) {
            continue;
        }

        const bool collected = std::abs(pickup.position.x - player.pos.x) < 36.0f && std::abs(pickup.position.laneY - player.pos.laneY) < 24.0f;
        if (!collected) {
            continue;
        }

        pickup.active = false;
        if (pickup.kind == PickupKind::Health) {
            player.hp = std::min(player.maxHp, player.hp + 20);
        } else if (pickup.kind == PickupKind::Energy) {
            m_playerEnergy = std::clamp(
                m_playerEnergy + 20.0f, 0.0f, kMaxPlayerEnergy);
        }
    }

    m_pickups.erase(std::remove_if(m_pickups.begin(), m_pickups.end(), [](const PickupVm& pickup) {
        return !pickup.active;
    }), m_pickups.end());
}

void GameSimulation::reset()
{
    populate_initial_state();
}

void GameSimulation::spawn_boss_if_needed()
{
    if (m_bossSpawned || m_bossDefeated || m_entities.empty()) {
        return;
    }

    const auto& player = m_entities.front();
    if (!player.alive || !m_bossIntroStarted || !regular_waves_cleared()) {
        return;
    }

    if (m_encounterTimer < kEncounterIntroSeconds) {
        return;
    }

    EntityState boss;
    boss.id = m_nextId++;
    boss.kind = EntityKind::Boss;
    boss.behaviorState = EnemyBehaviorState::Patrol;
    boss.hp = kBossHealth;
    boss.maxHp = kBossHealth;
    boss.pos.x = std::min(kWorldWidth - 80.0f,
                          kBossTriggerX + kBossSpawnOffsetX);
    boss.pos.laneY = std::clamp(player.pos.laneY, kStreetTop, kStreetBottom);
    boss.alive = true;
    m_entities.push_back(boss);
    m_bossSpawned = true;
}

void GameSimulation::update_boss_state() noexcept
{
    if (!m_bossSpawned) {
        return;
    }

    const auto boss = std::find_if(m_entities.begin(), m_entities.end(), [](const EntityState& entity) {
        return entity.kind == EntityKind::Boss;
    });
    if (boss == m_entities.end()) return;

    m_bossDefeated = !boss->alive || boss->hp <= 0;
    m_bossVictoryReady = m_bossDefeated && boss->deathTimer <= 0.0f;
}

void GameSimulation::apply_damage(EntityState& target, int amount,
                                  alleyfist::ImpactLevel impact) noexcept
{
    if (!target.alive || amount <= 0) {
        return;
    }
    if (target.kind == EntityKind::Player && target.hurtTimer > 0.0f) {
        return;
    }

    target.hp -= amount;
    target.hurtTimer = kHurtDuration;
    target.behaviorState = EnemyBehaviorState::Hurt;
    target.attackTimer = 0.0f;
    target.chargeTimer = 0.0f;
    target.attackHitApplied = true;
    target.impactRevision += 1;
    target.lastImpact = impact;

    if (target.kind == EntityKind::Player) {
        m_jumpActive = false;
        m_jumpElapsed = 0.0f;
        m_attackTimer = 0.0f;
        m_attackElapsed = 0.0f;
        m_attackHitApplied = false;
        m_activePlayerAction = PlayerActionType::None;
        target.pos.z = 0.0f;
    }

    if (target.hp <= 0) {
        target.hp = 0;
        target.alive = false;
        if (target.kind != EntityKind::Player) {
            target.deathTimer = kEnemyDeathDisplaySeconds;
        }
    }
}

void GameSimulation::spawn_ranged_projectile(const EntityState& owner)
{
    if (m_entities.empty()) {
        return;
    }

    ProjectileVm projectile;
    projectile.id = m_nextProjectileId++;
    projectile.visualVariant = owner.visualVariant;
    const auto& player = m_entities.front();
    const float facingDirection = owner.pos.x < player.pos.x ? 1.0f : -1.0f;
    projectile.position = owner.pos;
    projectile.position.x += facingDirection * 24.0f;
    projectile.position.z = 36.0f;
    projectile.facing = facingDirection > 0.0f ? alleyfist::Facing::Right
                                               : alleyfist::Facing::Left;
    const float dx = player.pos.x - projectile.position.x;
    const float dy = player.pos.laneY - projectile.position.laneY;
    const float flightSeconds = std::clamp(
        std::abs(dx) / kProjectileSpeed, 0.25f, 1.8f);
    projectile.velocityX = dx / flightSeconds;
    projectile.velocityLaneY = dy / flightSeconds;
    projectile.velocityZ =
        (player.pos.z - projectile.position.z +
         0.5f * kProjectileGravity * flightSeconds * flightSeconds) /
        flightSeconds;
    projectile.lifeSeconds = std::min(2.4f, flightSeconds + 0.75f);
    m_projectiles.push_back(projectile);
}

void GameSimulation::spawn_pickup(const alleyfist::WorldPosition& position, alleyfist::PickupKind kind)
{
    PickupVm pickup;
    pickup.id = m_nextPickupId++;
    pickup.kind = kind;
    pickup.position = position;
    pickup.position.z = 8.0f;
    pickup.lifeSeconds = kPickupLifetime;
    m_pickups.push_back(pickup);
}

} // namespace alleyfist::viewmodel
