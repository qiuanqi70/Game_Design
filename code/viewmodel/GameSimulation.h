#pragma once

#include "../common/game_state.h"
#include "SimulationTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace alleyfist::viewmodel {

class GameSimulation {
public:
    GameSimulation();
    ~GameSimulation();

    // 注册/移除时钟监听器（返回 cookie）
    std::uintptr_t add_tick_listener(std::function<void(float)>&& cb);
    void remove_tick_listener(std::uintptr_t cookie) noexcept;

    // 启停与单步推进（由 ViewModel 或外部时钟驱动）
    void start();
    void stop();
    void step(float deltaSeconds);

    // 访问当前实体状态
    const EntityList& entities() const noexcept { return m_entities; }
    bool boss_spawned() const noexcept { return m_bossSpawned; }
    bool boss_defeated() const noexcept { return m_bossDefeated; }
    bool boss_victory_ready() const noexcept { return m_bossVictoryReady; }
    const EncounterState& encounter_state() const noexcept { return m_encounter; }
    const std::vector<ProjectileState>& projectiles() const noexcept { return m_projectileStates; }
    const std::vector<PickupState>& pickups() const noexcept { return m_pickupStates; }
    float player_energy() const noexcept { return m_playerEnergy; }
    void set_player_energy(float energy) noexcept;

    // 控制接口（由 ViewModel 的命令绑定）
    void player_move(float dx, float dy) noexcept;
    void player_attack() noexcept;
    void reset() noexcept;

private:
    enum class WavePhase {
        Fighting,
        Transition,
        Complete
    };

    struct MovementBounds {
        float minimumX = 0.0f;
        float maximumX = 0.0f;
    };

    std::vector<std::function<void(float)>> m_tickListeners;
    EntityList m_entities;
    std::vector<ProjectileVm> m_projectiles;
    std::vector<PickupVm> m_pickups;
    std::vector<ProjectileState> m_projectileStates;
    std::vector<PickupState> m_pickupStates;
    bool m_running = false;
    bool m_bossSpawned = false;
    bool m_bossDefeated = false;
    bool m_bossVictoryReady = false;
    bool m_bossIntroStarted = false;
    int m_nextId = 1;
    std::uint32_t m_nextProjectileId = 1;
    std::uint32_t m_nextPickupId = 1;
    float m_elapsedSeconds = 0.0f;
    float m_encounterTimer = 0.0f;
    float m_playerEnergy = 100.0f;
    EncounterState m_encounter;
    std::uint32_t m_currentWave = 0;
    WavePhase m_wavePhase = WavePhase::Fighting;
    float m_waveTransitionTimer = 0.0f;

    void notify_tick(float dt);
    void populate_initial_state();
    void update_encounter_state(float dt);
    void update_wave_progression(float dt);
    void spawn_wave_enemies(std::uint32_t waveIndex);
    std::uint32_t alive_regular_enemy_count() const noexcept;
    bool regular_waves_cleared() const noexcept;
    MovementBounds player_movement_bounds() const noexcept;
    void simulate_ai(float dt);
    void update_death_presentations(float dt) noexcept;
    void update_projectiles(float dt);
    void update_pickups(float dt);
    void spawn_boss_if_needed();
    void update_boss_state() noexcept;
    void apply_damage(EntityState& target, int amount, alleyfist::ImpactLevel impact, int sourceId);
    void spawn_ranged_projectile(const EntityState& owner);
    void spawn_pickup(const alleyfist::WorldPosition& position, alleyfist::PickupKind kind);
    void sync_shared_state();
};

} // namespace alleyfist::viewmodel
