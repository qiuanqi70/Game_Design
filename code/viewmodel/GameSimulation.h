#pragma once

#include "../common/game_state.h"
#include "SimulationTypes.h"

#include <cstdint>
#include <vector>

namespace alleyfist::viewmodel {

/// @brief ViewModel 内部的轻量规则模拟器。
///
/// 该类不依赖 Qt，也不暴露给 View。它负责玩家移动、攻击判定、敌人 AI、
/// 波次推进、Boss 遭遇、投射物和拾取物等规则；GameViewModel 再把这些内部
/// 类型映射成 Common 层 GameState，保证规则和绘制解耦。
class GameSimulation {
public:
    GameSimulation();
    ~GameSimulation() = default;

    GameSimulation(const GameSimulation&) = delete;
    GameSimulation& operator=(const GameSimulation&) = delete;

    // 单步推进由 ViewModel 的 tick 命令驱动。
    void step(float deltaSeconds);

    // 当前规则状态只读快照，由 GameViewModel 映射到 Common。
    const EntityList& entities() const noexcept { return m_entities; }
    bool boss_victory_ready() const noexcept { return m_bossVictoryReady; }
    const EncounterState& encounter_state() const noexcept { return m_encounter; }
    const std::vector<ProjectileVm>& projectiles() const noexcept { return m_projectiles; }
    const std::vector<PickupVm>& pickups() const noexcept { return m_pickups; }
    float elapsed_seconds() const noexcept { return m_elapsedSeconds; }
    float world_width() const noexcept { return kWorldWidth; }
    float street_top() const noexcept { return kStreetTop; }
    float street_bottom() const noexcept { return kStreetBottom; }
    float player_energy() const noexcept { return m_playerEnergy; }
    float player_max_energy() const noexcept { return kMaxPlayerEnergy; }
    bool player_exhausted() const noexcept { return m_exhaustedWarningTimer > 0.0f; }
    PlayerBehaviorState player_behavior_state() const noexcept;

    // 输入状态和动作请求由 GameViewModel 的命令直接转发。
    void set_move_left(bool pressed) noexcept { m_moveLeft = pressed; }
    void set_move_right(bool pressed) noexcept { m_moveRight = pressed; }
    void set_move_up(bool pressed) noexcept { m_moveUp = pressed; }
    void set_move_down(bool pressed) noexcept { m_moveDown = pressed; }
    void clear_movement_input() noexcept;
    bool request_player_action(PlayerActionType action) noexcept;
    void reset();

private:
    enum class WavePhase {
        Fighting,
        Transition,
        Complete
    };

    // 用于限制玩家在当前波次或 Boss 场景中的横向可移动范围。
    struct MovementBounds {
        float minimumX = 0.0f;
        float maximumX = 0.0f;
    };

    static constexpr float kMaxPlayerEnergy = 100.0f;
    static constexpr float kWorldWidth = 3000.0f;
    static constexpr float kStreetTop = 300.0f;
    static constexpr float kStreetBottom = 500.0f;

    EntityList m_entities;
    std::vector<ProjectileVm> m_projectiles;
    std::vector<PickupVm> m_pickups;
    bool m_bossSpawned = false;
    bool m_bossDefeated = false;
    bool m_bossVictoryReady = false;
    bool m_bossIntroStarted = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_jumpActive = false;
    int m_nextId = 1;
    std::uint32_t m_nextProjectileId = 1;
    std::uint32_t m_nextPickupId = 1;
    float m_elapsedSeconds = 0.0f;
    float m_encounterTimer = 0.0f;
    float m_playerEnergy = kMaxPlayerEnergy;
    float m_jumpElapsed = 0.0f;
    float m_attackTimer = 0.0f;
    float m_attackElapsed = 0.0f;
    float m_exhaustedWarningTimer = 0.0f;
    EncounterState m_encounter;
    std::uint32_t m_currentWave = 0;
    WavePhase m_wavePhase = WavePhase::Fighting;
    float m_waveTransitionTimer = 0.0f;
    PlayerActionType m_activePlayerAction = PlayerActionType::None;
    bool m_attackHitApplied = false;

    // 初始化 / 玩家输入与玩家状态
    void populate_initial_state();
    void update_player_state(float dt) noexcept;
    void move_player(float dt) noexcept;
    bool try_spend_energy(float cost) noexcept;
    void resolve_player_attack(PlayerActionType action) noexcept;

    // 波次、遭遇与边界控制
    void update_encounter_state(float dt);
    void update_wave_progression(float dt);
    void spawn_wave_enemies(std::uint32_t waveIndex);
    std::uint32_t alive_regular_enemy_count() const noexcept;
    bool regular_waves_cleared() const noexcept;
    MovementBounds player_movement_bounds() const noexcept;

    // 敌人行为树入口和各敌人类型的具体行为
    void simulate_ai(float dt);
    void update_patroller(EntityState& enemy, EntityState& player, float dt);
    void update_ambusher(EntityState& enemy, EntityState& player, float dt);
    void update_charger(EntityState& enemy, EntityState& player, float dt);
    void update_ranged_enemy(EntityState& enemy, EntityState& player, float dt);
    void update_boss(EntityState& enemy, EntityState& player, float dt);
    void hit_player_once(EntityState& enemy, EntityState& player,
                         int damage, alleyfist::ImpactLevel impact);

    // 战斗结果、临时对象和奖励
    void process_enemy_deaths();
    void update_death_presentations(float dt) noexcept;
    void update_projectiles(float dt);
    void update_pickups(float dt);
    void spawn_boss_if_needed();
    void update_boss_state() noexcept;
    void apply_damage(EntityState& target, int amount,
                      alleyfist::ImpactLevel impact) noexcept;
    void spawn_ranged_projectile(const EntityState& owner);
    void spawn_pickup(const alleyfist::WorldPosition& position, alleyfist::PickupKind kind);
};

} // namespace alleyfist::viewmodel
