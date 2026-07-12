#pragma once

#include "../common/actions.h"
#include "../common/snapshot.h"
#include "SimulationTypes.h"

#include <deque>
#include <vector>


namespace alleyfist {

// GameSimulation 是 ViewModel 内部的游戏模拟核心。
// 它可以拥有规则参数、AI、碰撞、攻击盒等内部类型，但只通过 GameSnapshot 向外暴露结果。
class GameSimulation {
public:
    GameSimulation();

    void reset();
    void step(float deltaSeconds, std::uint64_t frameIndex); //推进游戏状态，处理 Tick 命令
    void handle_command(const GameCommand& command); //处理input命令，处理玩家输入

    const GameSnapshot& snapshot() const noexcept { return m_snapshot; }

private:
    // 推图锁是模拟内部状态，View 只看到边界、镜头和 GO 提示。
    enum class ScrollLockState {
        Free,
        LockedByEncounter,
        LockedByBoss,
        LevelFinished
    };

    // gameplay logic
    void reset_gameplay(GamePhase phase);
    void update_player(float dt);
    void update_enemies(float dt);
    void apply_attacks();
    void check_encounters();
    void process_input_command(const GameCommand& command);
    void apply_movement(float dt);
    void begin_jump();
    void begin_attack(AttackKind attackKind, bool fromAir);
    void update_camera();
    void update_progress();
    void spawn_grunt_encounter();
    void spawn_boss_encounter();
    void clear_active_encounter();
    bool is_gameplay_active() const noexcept;
    bool is_player_action_locked() const noexcept;

    // helpers
    static bool rects_intersect(const Rect& a, const Rect& b) noexcept; //判断两个矩形是否相交，用于碰撞检测
    Rect actor_body_rect(const ActorSnapshot& actor) const noexcept;
    Rect combat_box_world_rect(const ActorSnapshot& owner, const CombatBox& box) const noexcept; //计算攻击盒在世界坐标系中的矩形，用于碰撞检测

    //命令队列，用于输入缓冲，存储来自 View 的输入命令
    std::deque<GameCommand> m_commandQueue;

    // 敌人攻击冷却，与 m_snapshot.enemies 保持同序。
    std::vector<float> m_enemyAttackTimers;

    GameRules m_rules;
    GameSnapshot m_snapshot;
    float m_accumulated = 0.0f; //累计时间，用于处理游戏逻辑的时间步长
    std::uint64_t m_frame = 0; //当前帧索引，用于跟踪游戏的帧数

    // 关卡流程控制字段留在 ViewModel 内部，不暴露给 View。
    std::uint32_t m_stageIndex = 0;
    EncounterId m_activeEncounterId = kInvalidEncounterId;
    bool m_bossSpawned = false;
    bool m_bossDefeated = false;
    ScrollLockState m_scrollLock = ScrollLockState::Free;

    //玩家的移动状态，表示玩家是否正在向左、向右、向上、向下移动，以及是否正在跳跃
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    bool m_jumpActive = false;
    bool m_pendingAttack = false;
    bool m_pendingAirAttack = false;
    AttackKind m_pendingAttackKind = AttackKind::None;
    float m_jumpElapsed = 0.0f;
    float m_playerActionTimer = 0.0f;
    float m_energyRecoveryCarry = 0.0f;
    float m_comboWindowSeconds = 0.35f;
    GamePhase m_phaseBeforePause = GamePhase::Playing;
};

} // namespace alleyfist
