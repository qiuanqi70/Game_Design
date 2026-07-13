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

    spawn_boss_if_needed();

    // 简单仿真：更新敌人靠近玩家，处理死亡
    simulate_ai(deltaSeconds);
    update_boss_state();

    // 通知监听器
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
    m_bossSpawned = false;
    m_bossDefeated = false;

    // 玩家为 id=1
    EntityState player;
    player.id = m_nextId++;
    player.kind = EntityKind::Player;
    player.hp = 100;
    player.maxHp = 100;
    player.pos.x = 80.0f;
    player.pos.laneY = 400.0f;
    player.alive = true;
    m_entities.push_back(player);

    // 几个小怪
    for (int i = 0; i < 3; ++i) {
        EntityState e;
        e.id = m_nextId++;
        e.kind = EntityKind::Grunt;
        e.hp = 20;
        e.maxHp = 20;
        e.pos.x = 320.0f + i * 80.0f; // 在玩家右侧
        e.pos.laneY = (i % 2 == 0) ? 380.0f : 420.0f;
        e.alive = true;
        m_entities.push_back(e);
    }
}

void GameSimulation::simulate_ai(float dt)
{
    if (m_entities.empty()) return;
    // 第一项为玩家
    auto& player = m_entities[0];

    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;

        // 向玩家靠近
        float dx = player.pos.x - e.pos.x;
        float dy = player.pos.laneY - e.pos.laneY;
        float dist = std::sqrt(dx * dx + dy * dy) + 0.0001f;
        float speed = e.kind == EntityKind::Boss ? 55.0f : 30.0f; // 单位：像素/秒
        e.pos.x += (dx / dist) * speed * dt;
        e.pos.laneY += (dy / dist) * speed * dt;

        // 简单碰撞伤害：如果靠近则互相掉血
        if (std::abs(e.pos.x - player.pos.x) < 10.0f && std::abs(e.pos.laneY - player.pos.laneY) < 10.0f) {
            player.hp -= e.kind == EntityKind::Boss ? 2 : 1;
            if (player.hp <= 0) player.alive = false;
        }
    }

    // 清理死亡实体（保持玩家在 index 0）
    for (auto& e : m_entities) {
        if (e.hp <= 0) e.alive = false;
    }
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

    // 对附近第一个敌人造成伤害
    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;
        const float hitRangeX = e.kind == EntityKind::Boss ? 85.0f : 55.0f;
        const float hitRangeY = e.kind == EntityKind::Boss ? 45.0f : 35.0f;
        if (std::abs(e.pos.x - p.pos.x) < hitRangeX && std::abs(e.pos.laneY - p.pos.laneY) < hitRangeY) {
            e.hp -= 10;
            if (e.hp <= 0) e.alive = false;
            update_boss_state();
            break;
        }
    }
}

void GameSimulation::reset() noexcept
{
    m_nextId = 1;
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
    if (!m_bossSpawned || m_bossDefeated) {
        return;
    }

    const bool bossAlive = std::any_of(m_entities.begin(), m_entities.end(), [](const EntityState& entity) {
        return entity.kind == EntityKind::Boss && entity.alive && entity.hp > 0;
    });
    m_bossDefeated = !bossAlive;
}

} // namespace alleyfist::viewmodel
