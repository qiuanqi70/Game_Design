#pragma once

#include "../common/snapshot.h"
#include "../common/actions.h"

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>


namespace alleyfist {

class GameSimulation {
public:
    GameSimulation();

    void reset();
    void step(float deltaSeconds, std::uint64_t frameIndex);
    void handle_command(const GameCommand& command);

    const GameSnapshot& snapshot() const noexcept { return m_snapshot; }

private:
    void update_player(float dt);
    void update_enemies(float dt);
    void apply_attacks();
    void check_encounters();
    void process_input_command(const GameCommand& command);
    void apply_movement(float dt);
    void begin_jump();
    void begin_attack(AttackKind attackKind, bool fromAir);

    // helpers
    static bool rects_intersect(const Rect& a, const Rect& b) noexcept;
    Rect combat_box_world_rect(const ActorViewData& owner, const CombatBoxViewData& box) const noexcept;

    // command queue for input buffering
    std::deque<GameCommand> m_commandQueue;

    // tracking which actor was hit in current frame to avoid multi-hit per attack frame
    std::unordered_set<ActorId> m_hitThisFrame;

    // enemy cooldown timers (attack cooldown)
    std::unordered_map<ActorId, float> m_enemyAttackTimers;

    GameRules m_rules;
    GameSnapshot m_snapshot;
    float m_accumulated = 0.0f;
    std::uint64_t m_frame = 0;

    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_jumpActive = false;
    bool m_pendingAttack = false;
    bool m_pendingAirAttack = false;
    AttackKind m_pendingAttackKind = AttackKind::None;
    float m_jumpElapsed = 0.0f;
    float m_comboWindowSeconds = 0.35f;
};

} // namespace alleyfist
