#include "GameSimulation.h"
#include <algorithm>
#include <cmath>

namespace alleyfist::viewmodel {

GameSimulation::GameSimulation()
{
    populate_initial_state();
}

GameSimulation::~GameSimulation() = default;

std::uintptr_t GameSimulation::add_tick_listener(TickCallback&& cb)
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

    // 简单仿真：更新敌人靠近玩家，处理死亡
    simulate_ai(deltaSeconds);

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

    // 玩家为 id=1
    EntityState player;
    player.id = m_nextId++;
    player.hp = 100;
    player.pos.x = 0.0f;
    player.pos.laneY = 0.0f;
    player.alive = true;
    m_entities.push_back(player);

    // 几个小怪
    for (int i = 0; i < 3; ++i) {
        EntityState e;
        e.id = m_nextId++;
        e.hp = 20;
        e.pos.x = 200.0f + i * 80.0f; // 在玩家右侧
        e.pos.laneY = (i % 2 == 0) ? -10.0f : 10.0f;
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
        float speed = 30.0f; // 单位：像素/秒
        e.pos.x += (dx / dist) * speed * dt;
        e.pos.laneY += (dy / dist) * speed * dt;

        // 简单碰撞伤害：如果靠近则互相掉血
        if (std::abs(e.pos.x - player.pos.x) < 10.0f && std::abs(e.pos.laneY - player.pos.laneY) < 10.0f) {
            player.hp -= 1;
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
    p.pos.x += dx;
    p.pos.laneY += dy;
}

void GameSimulation::player_attack() noexcept
{
    if (m_entities.size() < 2) return;
    auto& p = m_entities[0];

    // 对附近第一个敌人造成伤害
    for (size_t i = 1; i < m_entities.size(); ++i) {
        auto& e = m_entities[i];
        if (!e.alive) continue;
        if (std::abs(e.pos.x - p.pos.x) < 30.0f && std::abs(e.pos.laneY - p.pos.laneY) < 20.0f) {
            e.hp -= 10;
            if (e.hp <= 0) e.alive = false;
            break;
        }
    }
}

void GameSimulation::reset() noexcept
{
    m_nextId = 1;
    populate_initial_state();
}

} // namespace alleyfist::viewmodel
