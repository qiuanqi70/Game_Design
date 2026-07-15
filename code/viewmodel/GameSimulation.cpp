#include "GameSimulation.h"
#include <algorithm>
#include <cmath>

namespace alleyfist::viewmodel {

namespace {

constexpr float kWorldWidth = 3000.0f;
constexpr float kStreetTop = 300.0f;
constexpr float kStreetBottom = 500.0f;
constexpr float kBossTriggerX = 2350.0f;
constexpr float kBossSpawnOffsetX = 260.0f;
constexpr int kBossHealth = 140;
constexpr float kEncounterIntroSeconds = 1.3f;
constexpr float kRangedAttackCooldown = 1.8f;
constexpr float kChargeDuration = 0.55f;
constexpr float kHurtDuration = 0.25f;
constexpr float kProjectileSpeed = 260.0f;
constexpr float kProjectileGravity = 80.0f;
constexpr float kPickupLifetime = 10.0f;

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
    update_encounter_state(deltaSeconds);
    spawn_boss_if_needed();
    simulate_ai(deltaSeconds);
    update_projectiles(deltaSeconds);
    update_pickups(deltaSeconds);
    update_boss_state();
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
    m_nextId = 1;
    m_nextProjectileId = 1;
    m_nextPickupId = 1;
    m_elapsedSeconds = 0.0f;
    m_encounterTimer = 0.0f;
    m_encounter = {};

    EntityState player;
    player.id = m_nextId++;
    player.kind = EntityKind::Player;
    player.hp = 100;
    player.maxHp = 100;
    player.pos.x = 80.0f;
    player.pos.laneY = 400.0f;
    player.alive = true;
    m_entities.push_back(player);

    for (int i = 0; i < 3; ++i) {
        EntityState e;
        e.id = m_nextId++;
        e.kind = EntityKind::Patroller;
        e.enemyType = EnemyType::Patroller;
        e.behaviorState = EnemyBehaviorState::Patrol;
        e.hp = 20;
        e.maxHp = 20;
        e.pos.x = 320.0f + i * 80.0f;
        e.pos.laneY = (i % 2 == 0) ? 380.0f : 420.0f;
        e.patrolRangeLeft = e.pos.x - 110.0f;
        e.patrolRangeRight = e.pos.x + 110.0f;
        e.alive = true;
        m_entities.push_back(e);
    }

    sync_shared_state();
}

void GameSimulation::update_encounter_state(float dt)
{
    if (m_bossDefeated) {
        m_encounter.kind = EncounterKind::None;
        m_encounter.phase = EncounterPhase::Cleared;
        m_encounter.currentWave = 0;
        m_encounter.totalWaves = 0;
        m_encounter.remainingEnemies = 0;
        m_encounter.introProgress = 1.0f;
        return;
    }

    if (m_bossSpawned) {
        m_encounter.kind = EncounterKind::Boss;
        m_encounter.phase = EncounterPhase::Fighting;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = 1;
        m_encounter.introProgress = 1.0f;
        return;
    }

    if (m_entities.empty()) {
        return;
    }

    const auto& player = m_entities.front();
    if (player.alive && player.pos.x >= kBossTriggerX) {
        m_encounterTimer += dt;
        m_encounter.kind = EncounterKind::Boss;
        m_encounter.phase = EncounterPhase::Intro;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = 1;
        m_encounter.introProgress = std::clamp(m_encounterTimer / kEncounterIntroSeconds, 0.0f, 1.0f);
    } else {
        m_encounter.kind = EncounterKind::EnemyWave;
        m_encounter.phase = EncounterPhase::Fighting;
        m_encounter.currentWave = 1;
        m_encounter.totalWaves = 1;
        m_encounter.remainingEnemies = static_cast<std::uint32_t>(std::max<int>(0, static_cast<int>(m_entities.size()) - 1));
        m_encounter.introProgress = 1.0f;
    }
}

void GameSimulation::simulate_ai(float dt)
{
    if (m_entities.empty()) return;
    auto& player = m_entities[0];

    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;

        if (e.hurtTimer > 0.0f) {
            e.hurtTimer = std::max(0.0f, e.hurtTimer - dt);
            if (e.hurtTimer <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Patrol;
            }
        }

        if (e.chargeTimer > 0.0f) {
            e.chargeTimer = std::max(0.0f, e.chargeTimer - dt);
            if (e.chargeTimer <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Patrol;
            }
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

        switch (e.kind) {
        case EntityKind::Patroller: {
            const float moveDir = (e.pos.x < e.patrolRangeLeft + 5.0f) ? 1.0f : (e.pos.x > e.patrolRangeRight - 5.0f ? -1.0f : 0.0f);
            if (moveDir == 0.0f) {
                e.pos.x += 18.0f * dt * (std::sin(m_elapsedSeconds * 0.9f) >= 0.0f ? 1.0f : -1.0f);
            } else {
                e.pos.x += 18.0f * dt * moveDir;
            }
            break;
        }
        case EntityKind::Ambusher: {
            if (std::abs(dx) < 220.0f && e.ambushCooldown <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Ambush;
                e.ambushCooldown = 1.8f;
                e.attackCooldown = 0.35f;
            }
            if (e.behaviorState == EnemyBehaviorState::Ambush && e.attackCooldown > 0.0f) {
                e.pos.x += 90.0f * dt;
            }
            break;
        }
        case EntityKind::Charger: {
            if (std::abs(dx) < 180.0f && e.attackCooldown <= 0.0f) {
                e.behaviorState = EnemyBehaviorState::Charge;
                e.chargeTimer = kChargeDuration;
                e.attackCooldown = 1.4f;
            }
            if (e.behaviorState == EnemyBehaviorState::Charge && e.chargeTimer > 0.0f) {
                e.pos.x += (dx / dist) * 220.0f * dt;
                e.pos.laneY += (dy / dist) * 220.0f * dt;
            }
            break;
        }
        case EntityKind::Ranged: {
            if (e.attackCooldown <= 0.0f && std::abs(dx) < 360.0f) {
                e.behaviorState = EnemyBehaviorState::RangedAttack;
                e.attackCooldown = kRangedAttackCooldown;
                spawn_ranged_projectile(e);
            }
            if (std::abs(dx) > 5.0f) {
                e.pos.x += (dx / dist) * 18.0f * dt;
            }
            break;
        }
        case EntityKind::Boss: {
            if (std::abs(dx) < 250.0f) {
                e.behaviorState = EnemyBehaviorState::Charge;
                e.chargeTimer = 0.12f;
            }
            e.pos.x += (dx / dist) * 40.0f * dt;
            e.pos.laneY += (dy / dist) * 40.0f * dt;
            break;
        }
        default:
            break;
        }

        if (std::abs(e.pos.x - player.pos.x) < 12.0f && std::abs(e.pos.laneY - player.pos.laneY) < 12.0f) {
            apply_damage(player, e.kind == EntityKind::Boss ? 2 : 1, ImpactLevel::Light, e.id);
        }
    }

    for (auto& e : m_entities) {
        if (e.hp <= 0) {
            e.alive = false;
            if (e.kind != EntityKind::Boss) {
                spawn_pickup(e.pos, PickupKind::Health);
            }
        }
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
        pickup.position.z = std::sin(m_elapsedSeconds * 2.2f + pickup.id * 0.6f) * 8.0f;
        if (pickup.lifeSeconds <= 0.0f) {
            pickup.active = false;
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

    EntityState boss;
    boss.id = m_nextId++;
    boss.kind = EntityKind::Boss;
    boss.enemyType = EnemyType::Boss;
    boss.behaviorState = EnemyBehaviorState::Charge;
    boss.hp = kBossHealth;
    boss.maxHp = kBossHealth;
    boss.pos.x = std::min(kWorldWidth - 80.0f, player.pos.x + kBossSpawnOffsetX);
    boss.pos.laneY = std::clamp(player.pos.laneY, kStreetTop, kStreetBottom);
    boss.alive = true;
    m_entities.push_back(boss);
    m_bossSpawned = true;
    m_encounterTimer = 0.0f;
}

void GameSimulation::update_boss_state() noexcept
{
    if (!m_bossSpawned || m_bossDefeated) {
        return;
    }

    const bool bossAlive = std::any_of(m_entities.begin(), m_entities.end(), [](const EntityState& entity) {
        return entity.kind == EntityKind::Boss && entity.alive && entity.hp > 0;
    });
    m_bossDefeated = !bossAlive;
}

void GameSimulation::apply_damage(EntityState& target, int amount, alleyfist::ImpactLevel impact, int sourceId)
{
    (void)sourceId;
    if (!target.alive) {
        return;
    }

    target.hp -= amount;
    target.hurtTimer = kHurtDuration;
    target.behaviorState = EnemyBehaviorState::Hurt;
    target.impactRevision += 1;
    target.lastImpact = impact;

    if (target.hp <= 0) {
        target.hp = 0;
        target.alive = false;
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
    projectile.position = owner.pos;
    projectile.position.z = 8.0f;
    projectile.facing = owner.pos.x < m_entities.front().pos.x ? alleyfist::Facing::Right : alleyfist::Facing::Left;
    const auto& player = m_entities.front();
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
