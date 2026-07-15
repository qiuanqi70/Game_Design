#include "GameSimulation.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace alleyfist::viewmodel {

namespace {

constexpr float kWorldWidth = 3000.0f;
constexpr float kStreetTop = 300.0f;
constexpr float kStreetBottom = 500.0f;
constexpr float kBossTriggerX = 2350.0f;
constexpr float kBossSpawnOffsetX = 260.0f;
constexpr int kBossHealth = 140;
constexpr float kEncounterIntroSeconds = 2.8f;
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

bool overlaps(const EntityState& first, const EntityState& second,
              float rangeX, float rangeY)
{
    return std::abs(first.pos.x - second.pos.x) < rangeX &&
           std::abs(first.pos.laneY - second.pos.laneY) < rangeY;
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

GameSimulation::~GameSimulation() = default;

std::uintptr_t GameSimulation::add_tick_listener(std::function<void(float)>&& cb)
{
    std::uintptr_t index = 0;
    for (auto& item : m_tickListeners) {
        if (!item) {
            item = std::move(cb);
            return index + 1;
        }
        ++index;
    }
    m_tickListeners.push_back(std::move(cb));
    return m_tickListeners.size();
}

void GameSimulation::remove_tick_listener(std::uintptr_t cookie) noexcept
{
    if (cookie == 0 || cookie > m_tickListeners.size()) return;
    m_tickListeners[cookie - 1] = nullptr;
}

void GameSimulation::start()
{
    m_running = true;
}

void GameSimulation::stop()
{
    m_running = false;
}

void GameSimulation::step(float deltaSeconds)
{
    if (!m_running) return;

    m_elapsedSeconds += deltaSeconds;
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
    sync_shared_state();

    notify_tick(deltaSeconds);
}

void GameSimulation::notify_tick(float dt)
{
    for (auto& cb : m_tickListeners) {
        if (cb) cb(dt);
    }
}

void GameSimulation::populate_initial_state()
{
    m_entities.clear();
    m_projectiles.clear();
    m_pickups.clear();
    m_projectileStates.clear();
    m_pickupStates.clear();
    m_bossSpawned = false;
    m_bossDefeated = false;
    m_bossVictoryReady = false;
    m_nextId = 1;
    m_nextProjectileId = 1;
    m_nextPickupId = 1;
    m_elapsedSeconds = 0.0f;
    m_encounterTimer = 0.0f;
    m_playerEnergy = 100.0f;
    m_encounter = {};
    m_currentWave = 0;
    m_waveTransitionTimer = 0.0f;

    EntityState player;
    player.id = m_nextId++;
    player.kind = EntityKind::Player;
    player.hp = 100;
    player.maxHp = 100;
    player.pos.x = 80.0f;
    player.pos.laneY = 400.0f;
    player.alive = true;
    m_entities.push_back(player);

    spawn_wave_enemies(0);

    sync_shared_state();
}

void GameSimulation::spawn_wave_enemies(std::uint32_t waveIndex)
{
    if (waveIndex > 2) return;

    constexpr std::array<std::array<EntityKind, 4>, 3> kWaveEnemies = {{
        {EntityKind::Patroller, EntityKind::Patroller, EntityKind::Patroller, EntityKind::Patroller},
        {EntityKind::Ambusher, EntityKind::Charger, EntityKind::Ranged, EntityKind::Patroller},
        {EntityKind::Ambusher, EntityKind::Charger, EntityKind::Ranged, EntityKind::Ambusher}
    }};

    constexpr std::array<std::array<float, 4>, 3> kWaveEnemyX = {{
        {320.0f, 420.0f, 520.0f, 620.0f},
        {350.0f, 450.0f, 550.0f, 650.0f},
        {380.0f, 480.0f, 580.0f, 680.0f}
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
        e.enemyType = [kind = e.kind]() {
            switch (kind) {
            case EntityKind::Ambusher:
                return EnemyType::Ambusher;
            case EntityKind::Charger:
                return EnemyType::Charger;
            case EntityKind::Ranged:
                return EnemyType::Ranged;
            case EntityKind::Boss:
                return EnemyType::Boss;
            case EntityKind::Patroller:
            case EntityKind::Player:
            default:
                return EnemyType::Patroller;
            }
        }();
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

void GameSimulation::update_wave_progression(float dt)
{
    if (m_bossSpawned || m_bossDefeated || m_entities.empty()) {
        return;
    }

    const auto& player = m_entities.front();
    if (!player.alive || player.pos.x >= kBossTriggerX) {
        return;
    }

    std::uint32_t aliveEnemyCount = 0;
    for (std::size_t i = 1; i < m_entities.size(); ++i) {
        if (m_entities[i].alive && m_entities[i].kind != EntityKind::Player) {
            ++aliveEnemyCount;
        }
    }

    if (aliveEnemyCount == 0 && m_currentWave < 2) {
        m_waveTransitionTimer += dt;
        if (m_waveTransitionTimer >= 1.0f) {
            m_currentWave++;
            m_waveTransitionTimer = 0.0f;
            spawn_wave_enemies(m_currentWave);
        }
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

    std::uint32_t remainingEnemies = 0;
    for (std::size_t i = 1; i < m_entities.size(); ++i) {
        if (m_entities[i].alive) {
            ++remainingEnemies;
        }
    }

    const auto& player = m_entities.front();
    if (player.alive && player.pos.x >= kBossTriggerX) {
        m_encounterTimer += dt;
        m_encounter.kind = EncounterKind::Boss;
        m_encounter.phase = EncounterPhase::Intro;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = remainingEnemies;
        m_encounter.introProgress = std::clamp(m_encounterTimer / kEncounterIntroSeconds, 0.0f, 1.0f);
        return;
    }

    if (remainingEnemies == 0) {
        if (m_currentWave < 2 && m_waveTransitionTimer > 0.0f) {
            m_encounter.kind = EncounterKind::EnemyWave;
            m_encounter.phase = EncounterPhase::Intro;
            m_encounter.currentWave = m_currentWave + 2;
            m_encounter.totalWaves = 3;
            m_encounter.remainingEnemies = 0;
            m_encounter.introProgress = std::clamp(m_waveTransitionTimer / 1.0f, 0.0f, 1.0f);
        } else {
            m_encounter.kind = EncounterKind::EnemyWave;
            m_encounter.phase = EncounterPhase::Cleared;
            m_encounter.currentWave = m_currentWave + 1;
            m_encounter.totalWaves = 3;
            m_encounter.remainingEnemies = 0;
            m_encounter.introProgress = 1.0f;
        }
    } else {
        m_encounter.kind = EncounterKind::EnemyWave;
        m_encounter.phase = EncounterPhase::Fighting;
        m_encounter.currentWave = m_currentWave + 1;
        m_encounter.totalWaves = 3;
        m_encounter.remainingEnemies = remainingEnemies;
        m_encounter.introProgress = 1.0f;
    }
}

void GameSimulation::simulate_ai(float dt)
{
    if (m_entities.empty()) return;
    auto& player = m_entities[0];

    if (player.hurtTimer > 0.0f) {
        player.hurtTimer = std::max(0.0f, player.hurtTimer - dt);
        if (player.hurtTimer <= 0.0f) {
            player.behaviorState = EnemyBehaviorState::Idle;
        }
    }

    const auto hitPlayerOnce = [this, &player](EntityState& enemy,
                                               int damage,
                                               ImpactLevel impact) {
        if (enemy.attackHitApplied ||
            !overlaps(enemy, player, kContactRangeX, kContactRangeY)) {
            return;
        }
        enemy.attackHitApplied = true;
        apply_damage(player, damage, impact, enemy.id);
    };

    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;

        if (e.hurtTimer > 0.0f) {
            e.hurtTimer = std::max(0.0f, e.hurtTimer - dt);
            if (e.hurtTimer > 0.0f) {
                continue;
            }
            e.behaviorState = default_behavior_for(e.kind);
        }

        if (e.ambushCooldown > 0.0f) {
            e.ambushCooldown = std::max(0.0f, e.ambushCooldown - dt);
        }

        if (e.attackCooldown > 0.0f) {
            e.attackCooldown = std::max(0.0f, e.attackCooldown - dt);
        }

        float dx = player.pos.x - e.pos.x;
        float dy = player.pos.laneY - e.pos.laneY;
        float dist = std::sqrt(dx * dx + dy * dy) + 0.0001f;
        if (std::abs(dx) > 0.001f) {
            e.facing = dx > 0.0f ? Facing::Right : Facing::Left;
        }

        switch (e.kind) {
        case EntityKind::Patroller: {
            if (e.behaviorState == EnemyBehaviorState::MeleeAttack) {
                e.attackTimer = std::max(0.0f, e.attackTimer - dt);
                if (!e.attackHitApplied && e.attackTimer <= kMeleeAttackHitTime) {
                    e.attackHitApplied = true;
                    if (overlaps(e, player, kContactRangeX, kContactRangeY)) {
                        apply_damage(player, 1, ImpactLevel::Light, e.id);
                    }
                }
                if (e.attackTimer <= 0.0f) {
                    e.behaviorState = EnemyBehaviorState::Patrol;
                }
                break;
            }

            const float moveDir = (e.pos.x < e.patrolRangeLeft + 5.0f) ? 1.0f : (e.pos.x > e.patrolRangeRight - 5.0f ? -1.0f : 0.0f);
            if (moveDir == 0.0f) {
                e.pos.x += 18.0f * dt * (std::sin(m_elapsedSeconds * 0.9f) >= 0.0f ? 1.0f : -1.0f);
            } else {
                e.pos.x += 18.0f * dt * moveDir;
            }

            if (e.attackCooldown <= 0.0f &&
                std::abs(dx) < 34.0f && std::abs(dy) < 24.0f) {
                e.behaviorState = EnemyBehaviorState::MeleeAttack;
                e.attackTimer = kMeleeAttackDuration;
                e.attackCooldown = kMeleeAttackCooldown;
                e.attackHitApplied = false;
            }
            break;
        }
        case EntityKind::Ambusher: {
            if (e.behaviorState == EnemyBehaviorState::Ambush) {
                e.attackTimer = std::max(0.0f, e.attackTimer - dt);
                if (e.attackTimer <= kAmbushDuration - kAttackWindup &&
                    e.attackTimer > 0.0f) {
                    e.pos.x += (dx / dist) * 90.0f * dt;
                    e.pos.laneY += (dy / dist) * 90.0f * dt;
                    hitPlayerOnce(e, 1, ImpactLevel::Light);
                }
                if (e.attackTimer <= 0.0f) {
                    e.behaviorState = EnemyBehaviorState::Idle;
                }
                break;
            }

            if (std::abs(dx) < 220.0f && std::abs(dy) < 120.0f &&
                e.ambushCooldown <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Ambush;
                e.ambushCooldown = 1.8f;
                e.attackTimer = kAmbushDuration;
                e.attackHitApplied = false;
            }
            break;
        }
        case EntityKind::Charger: {
            if (e.behaviorState == EnemyBehaviorState::Charge) {
                if (e.attackTimer > 0.0f) {
                    e.attackTimer = std::max(0.0f, e.attackTimer - dt);
                    break;
                }

                e.chargeTimer = std::max(0.0f, e.chargeTimer - dt);
                if (e.chargeTimer > 0.0f) {
                    e.pos.x += (dx / dist) * 220.0f * dt;
                    e.pos.laneY += (dy / dist) * 220.0f * dt;
                    hitPlayerOnce(e, 1, ImpactLevel::Heavy);
                } else {
                    e.behaviorState = EnemyBehaviorState::Idle;
                }
                break;
            }

            if (std::abs(dx) < 180.0f && std::abs(dy) < 90.0f &&
                e.attackCooldown <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Charge;
                e.chargeTimer = kChargeDuration;
                e.attackTimer = kChargeWindup;
                e.attackCooldown = 1.4f;
                e.attackHitApplied = false;
            }
            break;
        }
        case EntityKind::Ranged: {
            if (e.behaviorState == EnemyBehaviorState::RangedAttack) {
                e.attackTimer = std::max(0.0f, e.attackTimer - dt);
                if (!e.attackHitApplied && e.attackTimer <= kRangedAttackDuration - kAttackWindup) {
                    spawn_ranged_projectile(e);
                    e.attackHitApplied = true;
                }
                if (e.attackTimer <= 0.0f) {
                    e.behaviorState = EnemyBehaviorState::Idle;
                }
                break;
            }

            if (e.attackCooldown <= 0.0f &&
                std::abs(dx) < 360.0f && std::abs(dy) < 160.0f) {
                e.behaviorState = EnemyBehaviorState::RangedAttack;
                e.attackCooldown = kRangedAttackCooldown;
                e.attackTimer = kRangedAttackDuration;
                e.attackHitApplied = false;
                break;
            }
            if (std::abs(dx) > 5.0f) {
                e.pos.x += (dx / dist) * 18.0f * dt;
            }
            break;
        }
        case EntityKind::Boss: {
            if (e.behaviorState == EnemyBehaviorState::Charge) {
                if (e.attackTimer > 0.0f) {
                    e.attackTimer = std::max(0.0f, e.attackTimer - dt);
                    break;
                }

                e.chargeTimer = std::max(0.0f, e.chargeTimer - dt);
                if (e.chargeTimer > 0.0f) {
                    e.pos.x += (dx / dist) * 180.0f * dt;
                    e.pos.laneY += (dy / dist) * 180.0f * dt;
                    hitPlayerOnce(e, 2, ImpactLevel::Heavy);
                } else {
                    e.behaviorState = EnemyBehaviorState::Patrol;
                }
                break;
            }

            if (std::abs(dx) < 150.0f && std::abs(dy) < 90.0f &&
                e.attackCooldown <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Charge;
                e.chargeTimer = kChargeDuration;
                e.attackTimer = kChargeWindup;
                e.attackCooldown = kBossChargeCooldown;
                e.attackHitApplied = false;
                break;
            }

            e.pos.x += (dx / dist) * 40.0f * dt;
            e.pos.laneY += (dy / dist) * 40.0f * dt;
            break;
        }
        default:
            break;
        }
    }

    for (auto& e : m_entities) {
        if (e.kind == EntityKind::Player || e.hp > 0 || e.pickupDropped) {
            continue;
        }

        if (e.alive) {
            e.alive = false;
            e.deathTimer = kEnemyDeathDisplaySeconds;
        }
        if (e.kind == EntityKind::Boss) {
            spawn_pickup(e.pos, PickupKind::Health);
            spawn_pickup(e.pos, PickupKind::Energy);
        } else {
            const bool dropEnergy = e.enemyType == EnemyType::Ranged;
            spawn_pickup(e.pos, dropEnergy ? PickupKind::Energy : PickupKind::Health);
        }
        e.pickupDropped = true;
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
        projectile.position.laneY += projectile.velocityY * dt;
        projectile.position.z += (projectile.velocityY * 0.25f) * dt;
        projectile.lifeSeconds -= dt;
        projectile.velocityY += kProjectileGravity * dt;

        if (m_entities.empty()) {
            projectile.active = false;
            ++it;
            continue;
        }

        auto& player = m_entities.front();
        if (std::abs(projectile.position.x - player.pos.x) < 30.0f && std::abs(projectile.position.laneY - player.pos.laneY) < 24.0f) {
            apply_damage(player, 8, ImpactLevel::Heavy, static_cast<int>(projectile.ownerId));
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
            m_playerEnergy = std::clamp(m_playerEnergy + 20.0f, 0.0f, 100.0f);
        }
    }

    m_pickups.erase(std::remove_if(m_pickups.begin(), m_pickups.end(), [](const PickupVm& pickup) {
        return !pickup.active;
    }), m_pickups.end());
}

void GameSimulation::player_move(float dx, float dy) noexcept
{
    if (m_entities.empty()) return;
    auto& p = m_entities[0];
    p.pos.x = std::clamp(p.pos.x + dx, 0.0f, kWorldWidth);
    p.pos.laneY = std::clamp(p.pos.laneY + dy, kStreetTop, kStreetBottom);
}

void GameSimulation::player_attack() noexcept
{
    if (m_entities.size() < 2) return;
    auto& p = m_entities[0];

    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;
        const float hitRangeX = e.kind == EntityKind::Boss ? 85.0f : 55.0f;
        const float hitRangeY = e.kind == EntityKind::Boss ? 45.0f : 35.0f;
        if (std::abs(e.pos.x - p.pos.x) < hitRangeX && std::abs(e.pos.laneY - p.pos.laneY) < hitRangeY) {
            apply_damage(e, 10, ImpactLevel::Heavy, p.id);
            break;
        }
    }

    update_boss_state();
    update_encounter_state(0.0f);
}

void GameSimulation::reset() noexcept
{
    populate_initial_state();
}

void GameSimulation::spawn_boss_if_needed()
{
    if (m_bossSpawned || m_bossDefeated || m_entities.empty()) {
        return;
    }

    const auto& player = m_entities.front();
    if (!player.alive || player.pos.x < kBossTriggerX) {
        return;
    }

    if (m_encounterTimer < kEncounterIntroSeconds) {
        return;
    }

    EntityState boss;
    boss.id = m_nextId++;
    boss.kind = EntityKind::Boss;
    boss.enemyType = EnemyType::Boss;
    boss.behaviorState = EnemyBehaviorState::Patrol;
    boss.hp = kBossHealth;
    boss.maxHp = kBossHealth;
    boss.pos.x = std::min(kWorldWidth - 80.0f, player.pos.x + kBossSpawnOffsetX);
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

void GameSimulation::set_player_energy(float energy) noexcept
{
    m_playerEnergy = std::clamp(energy, 0.0f, 100.0f);
}

void GameSimulation::apply_damage(EntityState& target, int amount, alleyfist::ImpactLevel impact, int sourceId)
{
    (void)sourceId;
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
    projectile.ownerId = owner.id;
    projectile.visualVariant = owner.visualVariant;
    const auto& player = m_entities.front();
    const float facingDirection = owner.pos.x < player.pos.x ? 1.0f : -1.0f;
    projectile.position = owner.pos;
    projectile.position.x += facingDirection * 24.0f;
    projectile.position.z = 36.0f;
    projectile.facing = facingDirection > 0.0f ? alleyfist::Facing::Right
                                               : alleyfist::Facing::Left;
    const float dx = player.pos.x - owner.pos.x;
    const float dy = player.pos.laneY - owner.pos.laneY;
    const float dist = std::sqrt(dx * dx + dy * dy) + 0.0001f;
    projectile.velocityX = (dx / dist) * kProjectileSpeed;
    projectile.velocityY = (dy / dist) * kProjectileSpeed * 0.2f - 40.0f;
    projectile.lifeSeconds = 2.4f;
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

void GameSimulation::sync_shared_state()
{
    m_projectileStates.clear();
    for (const auto& projectile : m_projectiles) {
        ProjectileState state;
        state.id = projectile.id;
        state.kind = ProjectileKind::ThrownObject;
        state.visualVariant = projectile.visualVariant;
        state.team = Team::Enemy;
        state.position = projectile.position;
        state.facing = projectile.facing;
        m_projectileStates.push_back(state);
    }

    m_pickupStates.clear();
    for (const auto& pickup : m_pickups) {
        PickupState state;
        state.kind = pickup.kind;
        state.id = pickup.id;
        state.position = pickup.position;
        m_pickupStates.push_back(state);
    }
}

} // namespace alleyfist::viewmodel
