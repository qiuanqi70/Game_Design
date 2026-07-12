#include "GameSimulation.h"

#include <cmath>
#include <algorithm>
#include <utility>

namespace alleyfist {

// 所有玩法规则集中在 Simulation：移动、攻击、敌人、遭遇战、胜负判定都在这里。
// View 层只接收最终快照，因此规则变化不会反向污染绘制层。

namespace {

// 游戏规则常量：触发点、遭遇战边界和攻击动作持续时间。
constexpr float kPi = 3.1415926535f;
constexpr EncounterId kGruntEncounterId = 1;
constexpr EncounterId kBossEncounterId = 2;
constexpr float kGruntTriggerX = 400.0f;
constexpr float kBossTriggerX = 2350.0f;
constexpr float kEncounterLeftPadding = 240.0f;
constexpr float kEncounterRightPadding = 360.0f;
constexpr float kLightAttackSeconds = 0.18f;
constexpr float kHeavyAttackSeconds = 0.28f;
constexpr float kAirAttackSeconds = 0.24f;

bool is_attack_state(ActorState state) noexcept
{
    return state == ActorState::LightAttack ||
           state == ActorState::HeavyAttack ||
           state == ActorState::ComboFinisher ||
           state == ActorState::AirAttack;
}

} // namespace

GameSimulation::GameSimulation()
{
    reset();
}

void GameSimulation::reset()
{
    reset_gameplay(GamePhase::Title);
}

void GameSimulation::reset_gameplay(GamePhase phase)
{
    m_snapshot = {};
    m_snapshot.phase = phase;
    m_snapshot.frameIndex = 0;
    m_snapshot.elapsedSeconds = 0.0f;

    // init player
    m_snapshot.player.kind = ActorKind::Player;
    m_snapshot.player.team = Team::Player;
    m_snapshot.player.position.x = 50.0f;
    m_snapshot.player.position.laneY = 400.0f;
    m_snapshot.player.position.z = 0.0f;
    m_snapshot.player.health = {100, 100};
    m_snapshot.player.energy = {100, 100};
    m_snapshot.player.state = ActorState::Idle;
    m_snapshot.player.drawSize = {64.0f, 96.0f};
    m_snapshot.hud.playerHealth = {100, 100};
    m_snapshot.hud.playerEnergy = {100, 100};
    m_snapshot.hud.playerExhausted = false;

    // simple map
    m_snapshot.map.worldWidth = 3000.0f;
    m_snapshot.map.viewportWidth = 960.0f;
    m_snapshot.map.viewportHeight = 540.0f;
    m_snapshot.map.streetTopY = 300.0f;
    m_snapshot.map.streetBottomY = 500.0f;
    m_snapshot.map.leftBoundaryX = 0.0f;
    m_snapshot.map.rightBoundaryX = m_snapshot.map.viewportWidth;
    m_snapshot.screenMessage.clear();

    m_snapshot.enemies.clear();
    m_snapshot.effects.clear();
    m_commandQueue.clear();
    m_enemyAttackTimers.clear();
    m_rules = GameRules();
    m_stageIndex = 0;
    m_activeEncounterId = kInvalidEncounterId;
    m_bossSpawned = false;
    m_bossDefeated = false;
    m_scrollLock = ScrollLockState::Free;
    m_moveLeft = false;
    m_moveRight = false;
    m_moveUp = false;
    m_moveDown = false;
    m_jumpActive = false;
    m_pendingAttack = false;
    m_pendingAirAttack = false;
    m_pendingAttackKind = AttackKind::None;
    m_jumpElapsed = 0.0f;
    m_playerActionTimer = 0.0f;
    m_energyRecoveryCarry = 0.0f;
    m_phaseBeforePause = GamePhase::Playing;
}

void GameSimulation::step(float deltaSeconds, std::uint64_t frameIndex)
{
    m_frame = frameIndex;
    m_snapshot.frameIndex = frameIndex;

    // 先消费 View 发来的输入命令，再推进本帧模拟。
    while (!m_commandQueue.empty()) {
        auto cmd = m_commandQueue.front();
        m_commandQueue.pop_front();
        if (cmd.type == CommandType::Input) {
            process_input_command(cmd);
        }
    }

    if (!is_gameplay_active()) {
        //update_camera();
        //update_progress();
        return;
    }

    // 每帧依次处理移动、角色状态、敌人 AI、攻击判定和遭遇战推进。
    m_snapshot.elapsedSeconds += deltaSeconds;
    apply_movement(deltaSeconds);
    update_player(deltaSeconds);
    update_enemies(deltaSeconds);
    apply_attacks();
    check_encounters();

    // 精力按时间恢复；恢复到可轻攻击时退出疲劳状态。
    if (m_snapshot.player.energy.maximum > 0 &&
        m_snapshot.player.energy.current < m_snapshot.player.energy.maximum) {
        m_energyRecoveryCarry += m_rules.energyRecoveryPerSecond * deltaSeconds;
        const int recovered = static_cast<int>(m_energyRecoveryCarry);
        if (recovered > 0) {
            m_snapshot.player.energy.current = std::min(m_snapshot.player.energy.maximum,
                                                        m_snapshot.player.energy.current + recovered);
            m_energyRecoveryCarry -= static_cast<float>(recovered);
        }
        if (m_snapshot.player.energy.current >= m_rules.lightAttackEnergyCost) {
            m_snapshot.hud.playerExhausted = false;
        }
    } else if (m_snapshot.player.energy.current >= m_snapshot.player.energy.maximum) {
        m_energyRecoveryCarry = 0.0f;
        m_snapshot.hud.playerExhausted = false;
    }

    // HUD 显示值跟随玩家当前资源。
    m_snapshot.hud.playerHealth = m_snapshot.player.health;
    m_snapshot.hud.playerEnergy = m_snapshot.player.energy;

    update_camera();
    update_progress();
}

void GameSimulation::handle_command(const GameCommand& command)
{
    if (command.type == CommandType::Input) {
        m_commandQueue.push_back(command);
        return;
    }
    if (command.type == CommandType::Tick) {
        return;
    }
}

void GameSimulation::update_player(float dt)
{
    // 玩家位置限制在世界和街道范围内。
    if (m_snapshot.player.position.x < 0.0f)
        m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth)
        m_snapshot.player.position.x = m_snapshot.map.worldWidth;

    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;

    m_snapshot.hud.playerHealth = m_snapshot.player.health;
    m_snapshot.hud.playerEnergy = m_snapshot.player.energy;

    // 连击窗口耗尽后重置连击步数。
    if (m_snapshot.hud.comboTimeLeftSeconds > 0.0f) {
        m_snapshot.hud.comboTimeLeftSeconds = std::max(0.0f, m_snapshot.hud.comboTimeLeftSeconds - dt);
        if (m_snapshot.hud.comboTimeLeftSeconds <= 0.0f) {
            m_snapshot.hud.comboStep = 0;
        }
    }

    // 跳跃高度用正弦曲线表现上升和落地。
    if (m_jumpActive) {
        m_jumpElapsed += dt;
        const float progress = std::min(1.0f, m_jumpElapsed / m_rules.jumpSeconds);
        m_snapshot.player.position.z = m_rules.jumpHeight * std::sin(kPi * progress);
        if (progress >= 1.0f) {
            m_jumpElapsed = 0.0f;
            m_jumpActive = false;
            m_snapshot.player.position.z = 0.0f;
            if (m_playerActionTimer <= 0.0f || !is_attack_state(m_snapshot.player.state)) {
                m_snapshot.player.state = ActorState::Idle;
            }
        } else if (!is_attack_state(m_snapshot.player.state)) {
            m_snapshot.player.state = ActorState::Jump;
        }
    }

    // 攻击动作到时后回到空中/待机状态。
    if (m_playerActionTimer > 0.0f) {
        m_playerActionTimer = std::max(0.0f, m_playerActionTimer - dt);
        if (m_playerActionTimer <= 0.0f && is_attack_state(m_snapshot.player.state)) {
            m_snapshot.player.state = m_jumpActive ? ActorState::Jump : ActorState::Idle;
        }
    }

}

void GameSimulation::update_enemies(float dt)
{
    std::vector<ActorSnapshot> alive;
    std::vector<float> aliveTimers;
    alive.reserve(m_snapshot.enemies.size());
    aliveTimers.reserve(m_snapshot.enemies.size());

    for (std::size_t index = 0; index < m_snapshot.enemies.size(); ++index) {
        auto e = m_snapshot.enemies[index];
        // 冷却数组和敌人列表保持同序；敌人死亡被过滤时也同步过滤冷却值。
        float timer = index < m_enemyAttackTimers.size() ? m_enemyAttackTimers[index] : 1.0f;

        if (e.state == ActorState::Dead || !e.visible || e.health.current <= 0) {
            continue;
        }
        if (e.state != ActorState::Hurt && e.state != ActorState::KnockedDown) {
            float dir = (m_snapshot.player.position.x < e.position.x) ? -1.0f : 1.0f;
            const float speed = (e.kind == ActorKind::Boss) ? m_rules.bossMoveSpeed : m_rules.enemyMoveSpeed;
            e.position.x += dir * speed * dt;
            e.position.laneY += (m_snapshot.player.position.laneY > e.position.laneY) ? 45.0f * dt : -45.0f * dt;
        }
        timer -= dt;
        // 冷却结束且距离足够近时，敌人对玩家造成一次接触伤害。
        if (timer <= 0.0f) {
            float dx = std::abs(e.position.x - m_snapshot.player.position.x);
            float dy = std::abs(e.position.laneY - m_snapshot.player.position.laneY);
            if (dx < 48.0f && dy < 42.0f && m_snapshot.player.position.z < 35.0f) {
                const int damage = (e.kind == ActorKind::Boss) ? 16 : 8;
                m_snapshot.player.health.current = std::max(0, m_snapshot.player.health.current - damage);
                m_snapshot.player.state = ActorState::Hurt;
                timer = 1.0f;
                if (m_snapshot.player.health.current <= 0) {
                    m_snapshot.player.state = ActorState::Dead;
                    m_snapshot.phase = GamePhase::GameOver;
                    m_snapshot.result.gameOverReason = GameOverReason::PlayerDefeated;
                    m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
                }
            }
        }
        if (e.position.x < 0.0f) e.position.x = 0.0f;
        if (e.position.x > m_snapshot.map.worldWidth) e.position.x = m_snapshot.map.worldWidth;
        if (e.position.laneY < m_snapshot.map.streetTopY) e.position.laneY = m_snapshot.map.streetTopY;
        if (e.position.laneY > m_snapshot.map.streetBottomY) e.position.laneY = m_snapshot.map.streetBottomY;
        if (e.state == ActorState::Hurt || e.state == ActorState::Spawn) {
            e.state = ActorState::Idle;
        }
        alive.push_back(e);
        aliveTimers.push_back(timer);
    }
    m_snapshot.enemies = std::move(alive);
    m_enemyAttackTimers = std::move(aliveTimers);
}

void GameSimulation::apply_attacks()
{
    if (m_pendingAttack) {
        AttackKind kind = m_pendingAttackKind;
        m_pendingAttack = false;
        bool fromAir = m_pendingAirAttack;
        m_pendingAirAttack = false;

        if (m_snapshot.hud.playerExhausted) {
            return;
        }

        begin_attack(kind, fromAir);

        CombatBox cb;
        // 根据攻击类型设置攻击盒和伤害。
        if (kind == AttackKind::LightPunch) {
            cb.damage = 20;
            cb.attack = AttackKind::LightPunch;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 18.0f : -108.0f, -72.0f, 90.0f, 72.0f };
        } else if (kind == AttackKind::HeavyStrike) {
            cb.damage = 40;
            cb.attack = AttackKind::HeavyStrike;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 22.0f : -147.0f, -82.0f, 125.0f, 86.0f };
        } else if (fromAir) {
            cb.damage = 30;
            cb.attack = AttackKind::JumpKick;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 20.0f : -120.0f, -56.0f, 100.0f, 64.0f };
        }

        // 用攻击盒和敌人的即时身体矩形做命中判定。
        Rect attackRect = combat_box_world_rect(m_snapshot.player, cb);
        for (auto& e : m_snapshot.enemies) {
            if (!e.visible) continue;
            if (e.team == m_snapshot.player.team) continue;
            Rect body = actor_body_rect(e);
            if (rects_intersect(attackRect, body)) {
                e.health.current = std::max(0, e.health.current - cb.damage);
                if (e.health.current <= 0) {
                    e.state = ActorState::Dead;
                    e.visible = false;
                    e.targetable = false;
                    ++m_snapshot.result.defeatedEnemies;
                    if (e.kind == ActorKind::Boss) {
                        m_bossDefeated = true;
                    }
                } else {
                    e.state = ActorState::Hurt;
                }
                m_snapshot.hud.comboStep = std::min(3u, m_snapshot.hud.comboStep + 1u);
                m_snapshot.hud.comboTimeLeftSeconds = m_comboWindowSeconds;
            }
        }

        // 攻击消耗精力，精力见底时进入疲劳状态。
        int cost = (kind == AttackKind::HeavyStrike) ? m_rules.heavyAttackEnergyCost : m_rules.lightAttackEnergyCost;
        if (m_snapshot.player.energy.current > 0) {
            m_snapshot.player.energy.current = std::max(0, m_snapshot.player.energy.current - cost);
            if (m_snapshot.player.energy.current == 0) {
                m_snapshot.hud.playerExhausted = true;
            }
        }

        update_progress();
    }
}

void GameSimulation::process_input_command(const GameCommand& command)
{
    const auto action = command.input.action;//比如InputAction::MoveLeft
    const auto state = command.input.state;//比如ButtonState::Pressed

    if (action == InputAction::Restart && state == ButtonState::Triggered) {
        reset_gameplay(GamePhase::Playing);
        return;
    }

    // 确认键在标题、胜负和暂停阶段承担不同的流程控制职责。
    if (action == InputAction::Confirm && state == ButtonState::Triggered) {
        if (m_snapshot.phase == GamePhase::Title ||
            m_snapshot.phase == GamePhase::GameOver ||
            m_snapshot.phase == GamePhase::Win) {
            reset_gameplay(GamePhase::Playing);
        } else if (m_snapshot.phase == GamePhase::Paused) {
            m_snapshot.phase = m_phaseBeforePause;
            m_snapshot.screenMessage.clear();
        }
        return;
    }

    if (action == InputAction::Pause && state == ButtonState::Triggered) {
        if (m_snapshot.phase == GamePhase::Paused) {
            m_snapshot.phase = m_phaseBeforePause;
            m_snapshot.screenMessage.clear();
        } else if (is_gameplay_active()) {
            m_phaseBeforePause = m_snapshot.phase;
            m_snapshot.phase = GamePhase::Paused;
            m_snapshot.screenMessage.clear();
        }
        return;
    }

    if (!is_gameplay_active()) {
        return;
    }

    // 移动键记录持续状态；攻击和跳跃记录为待处理动作，由后续帧统一执行。
    if (action == InputAction::MoveLeft) {
        m_moveLeft = (state == ButtonState::Pressed);
    } else if (action == InputAction::MoveRight) {
        m_moveRight = (state == ButtonState::Pressed);
    } else if (action == InputAction::MoveUp) {
        m_moveUp = (state == ButtonState::Pressed);
    } else if (action == InputAction::MoveDown) {
        m_moveDown = (state == ButtonState::Pressed);
    } else if (action == InputAction::Jump && state == ButtonState::Triggered) {
        if (!m_jumpActive && !m_snapshot.hud.playerExhausted && m_snapshot.player.position.z <= 0.0f) {
            begin_jump();
        }
    } else if (action == InputAction::LightAttack && state == ButtonState::Triggered) {
        if (!m_snapshot.hud.playerExhausted && !is_player_action_locked()) {
            if (m_snapshot.player.position.z > 0.0f) {
                m_pendingAttack = true;
                m_pendingAirAttack = true;
                m_pendingAttackKind = AttackKind::JumpKick;
            } else {
                m_pendingAttack = true;
                m_pendingAttackKind = AttackKind::LightPunch;
            }
        }
    } else if (action == InputAction::HeavyAttack && state == ButtonState::Triggered) {
        if (!m_snapshot.hud.playerExhausted && !is_player_action_locked()) {
            m_pendingAttack = true;
            m_pendingAttackKind = AttackKind::HeavyStrike;
        }
    }
}

void GameSimulation::apply_movement(float dt)
{
    if (!is_gameplay_active() || m_snapshot.player.state == ActorState::Dead)
        return;

    float vx = 0.0f;
    float vy = 0.0f;
    // 根据当前按键状态计算本帧位移；攻击锁定时禁止地面移动。
    if (!is_player_action_locked() || m_jumpActive) {
        if (m_moveLeft) vx -= m_rules.playerMoveSpeed * dt;
        if (m_moveRight) vx += m_rules.playerMoveSpeed * dt;
        if (m_moveUp) vy -= m_rules.playerMoveSpeed * 0.6f * dt;
        if (m_moveDown) vy += m_rules.playerMoveSpeed * 0.6f * dt;
    }

    // 有位移时更新位置、状态和朝向；否则在非动作锁定时回到待机。
    if (std::abs(vx) > 0.0f || std::abs(vy) > 0.0f) {
        m_snapshot.player.position.x += vx;
        m_snapshot.player.position.laneY += vy;
        if (!is_player_action_locked()) {
            m_snapshot.player.state = m_jumpActive ? ActorState::Jump : ActorState::Walk;
        }
        if (vx < 0.0f) {
            m_snapshot.player.facing = Facing::Left;
        } else if (vx > 0.0f) {
            m_snapshot.player.facing = Facing::Right;
        }
    } else if (!is_player_action_locked() && !m_jumpActive) {
        m_snapshot.player.state = ActorState::Idle;
    }

    // 遭遇战/Boss 战期间限制玩家离开锁屏范围。
    if (m_snapshot.player.position.x < 0.0f) m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth) m_snapshot.player.position.x = m_snapshot.map.worldWidth;
    if (m_scrollLock == ScrollLockState::LockedByEncounter ||
        m_scrollLock == ScrollLockState::LockedByBoss) {
        m_snapshot.player.position.x = std::clamp(m_snapshot.player.position.x,
                                                  m_snapshot.map.leftBoundaryX,
                                                  m_snapshot.map.rightBoundaryX);
    }
    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY) m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY) m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;
}

void GameSimulation::begin_jump()
{
    // 初始化跳跃状态，并立即扣除跳跃精力。
    m_jumpActive = true;
    m_snapshot.player.position.z = 0.1f;
    m_snapshot.player.state = ActorState::Jump;
    m_jumpElapsed = 0.0f;
    m_snapshot.player.energy.current = std::max(0, m_snapshot.player.energy.current - m_rules.jumpEnergyCost);
    if (m_snapshot.player.energy.current == 0) {
        m_snapshot.hud.playerExhausted = true;
    }
}

void GameSimulation::begin_attack(AttackKind attackKind, bool fromAir)
{
    // 根据攻击类型设置对外可绘制的角色状态和动作锁定时长。
    if (fromAir || attackKind == AttackKind::JumpKick) {
        m_snapshot.player.state = ActorState::AirAttack;
        if (!m_jumpActive) {
            m_snapshot.player.position.z = m_rules.jumpHeight * 0.45f;
        }
        m_playerActionTimer = kAirAttackSeconds;
    } else if (attackKind == AttackKind::HeavyStrike) {
        m_snapshot.player.state = ActorState::HeavyAttack;
        m_playerActionTimer = kHeavyAttackSeconds;
    } else {
        m_snapshot.player.state = ActorState::LightAttack;
        m_playerActionTimer = kLightAttackSeconds;
    }
}

void GameSimulation::check_encounters()
{
    if (!is_gameplay_active()) {
        return;
    }

    // 若处于遭遇战，等待场上敌人清空后解除锁屏。
    if (m_activeEncounterId != kInvalidEncounterId) {
        const bool anyAlive = std::any_of(m_snapshot.enemies.begin(), m_snapshot.enemies.end(),
            [](const ActorSnapshot& actor) {
                return actor.team == Team::Enemy && actor.visible && actor.health.current > 0;
            });
        if (!anyAlive) {
            clear_active_encounter();
        }
        return;
    }

    // 根据玩家推进位置触发第一波小怪和 Boss 战。
    if (m_stageIndex == 0 && m_snapshot.player.position.x >= kGruntTriggerX) {
        spawn_grunt_encounter();
        return;
    }

    if (!m_bossSpawned && m_snapshot.player.position.x >= kBossTriggerX) {
        spawn_boss_encounter();
        return;
    }

    // Boss 已击败且玩家推进到关底时判定胜利。
    if (m_bossDefeated &&
        m_snapshot.player.position.x >= m_snapshot.map.worldWidth - 120.0f) {
        m_snapshot.phase = GamePhase::Win;
        m_scrollLock = ScrollLockState::LevelFinished;
        m_snapshot.result.winReason = WinReason::BossDefeated;
        m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
        m_snapshot.screenMessage.clear();
    }
}

void GameSimulation::update_camera()
{
    // 默认让镜头略微跟在玩家身后，锁屏时限制在遭遇战边界内。
    const float maxCamera = std::max(0.0f, m_snapshot.map.worldWidth - m_snapshot.map.viewportWidth);
    float desired = m_snapshot.player.position.x - m_snapshot.map.viewportWidth * 0.42f;

    if (m_scrollLock == ScrollLockState::LockedByEncounter ||
        m_scrollLock == ScrollLockState::LockedByBoss) {
        const float lockWidth = m_snapshot.map.rightBoundaryX - m_snapshot.map.leftBoundaryX;
        if (lockWidth >= m_snapshot.map.viewportWidth) {
            desired = std::clamp(desired,
                                 m_snapshot.map.leftBoundaryX,
                                 m_snapshot.map.rightBoundaryX - m_snapshot.map.viewportWidth);
        }
    }

    m_snapshot.map.cameraX = std::clamp(desired, 0.0f, maxCamera);
}

void GameSimulation::update_progress()
{
    // 进度条只暴露给 View 一个比例；Boss 血条则根据可见 Boss 自动同步。
    if (m_snapshot.map.worldWidth > 0.0f) {
        m_snapshot.progressRatio = std::clamp(m_snapshot.player.position.x / m_snapshot.map.worldWidth,
                                              0.0f,
                                              1.0f);
    }

    m_snapshot.hud.showBossHealth = false;
    for (const auto& enemy : m_snapshot.enemies) {
        if (enemy.kind == ActorKind::Boss && enemy.visible && enemy.health.maximum > 0) {
            m_snapshot.hud.showBossHealth = true;
            m_snapshot.hud.bossHealth = enemy.health;
            break;
        }
    }
}

void GameSimulation::spawn_grunt_encounter()
{
    // 进入小怪遭遇战：锁住推进范围，并刷出三名普通敌人。
    m_activeEncounterId = kGruntEncounterId;
    m_snapshot.phase = GamePhase::EncounterLocked;
    m_scrollLock = ScrollLockState::LockedByEncounter;
    m_snapshot.map.showGoIndicator = false;
    m_snapshot.map.leftBoundaryX = std::max(0.0f, m_snapshot.player.position.x - kEncounterLeftPadding);
    m_snapshot.map.rightBoundaryX = std::min(m_snapshot.map.worldWidth, m_snapshot.player.position.x + kEncounterRightPadding);
    m_snapshot.screenMessage.clear();

    for (int i = 0; i < 3; ++i) {
        ActorSnapshot enemy;
        enemy.kind = ActorKind::Grunt;
        enemy.team = Team::Enemy;
        enemy.position.x = m_snapshot.player.position.x + 105.0f + i * 55.0f;
        enemy.position.laneY = std::clamp(m_snapshot.player.position.laneY + ((i % 2 == 0) ? -22.0f : 22.0f),
                                          m_snapshot.map.streetTopY,
                                          m_snapshot.map.streetBottomY);
        enemy.health = {30, 30};
        enemy.state = ActorState::Spawn;
        enemy.visible = true;
        enemy.drawSize = {48.0f, 72.0f};
        m_snapshot.enemies.push_back(enemy);
        m_enemyAttackTimers.push_back(0.8f + i * 0.2f);
    }
}

void GameSimulation::spawn_boss_encounter()
{
    // 进入 Boss 遭遇战：记录 Boss 已刷出，避免重复触发。
    m_activeEncounterId = kBossEncounterId;
    m_bossSpawned = true;
    m_snapshot.phase = GamePhase::EncounterLocked;
    m_scrollLock = ScrollLockState::LockedByBoss;
    m_snapshot.map.showGoIndicator = false;
    m_snapshot.map.leftBoundaryX = std::max(0.0f, m_snapshot.player.position.x - kEncounterLeftPadding);
    m_snapshot.map.rightBoundaryX = std::min(m_snapshot.map.worldWidth, m_snapshot.player.position.x + kEncounterRightPadding);
    m_snapshot.screenMessage.clear();

    ActorSnapshot boss;
    boss.kind = ActorKind::Boss;
    boss.team = Team::Enemy;
    boss.position.x = std::min(m_snapshot.map.worldWidth - 120.0f, m_snapshot.player.position.x + 220.0f);
    boss.position.laneY = m_snapshot.player.position.laneY;
    boss.health = {140, 140};
    boss.state = ActorState::Spawn;
    boss.visible = true;
    boss.drawSize = {82.0f, 124.0f};
    m_snapshot.enemies.push_back(boss);
    m_enemyAttackTimers.push_back(1.0f);
    update_progress();
}

void GameSimulation::clear_active_encounter()
{
    // 清除当前遭遇战；若清除的是 Boss 战则直接进入胜利流程。
    const bool bossCleared = m_activeEncounterId == kBossEncounterId;

    m_activeEncounterId = kInvalidEncounterId;
    m_scrollLock = ScrollLockState::Free;
    m_snapshot.map.leftBoundaryX = 0.0f;
    m_snapshot.map.rightBoundaryX = m_snapshot.map.worldWidth;
    m_snapshot.map.showGoIndicator = true;

    if (bossCleared) {
        m_stageIndex = 2;
        m_bossDefeated = true;
        m_snapshot.hud.showBossHealth = false;
        m_snapshot.phase = GamePhase::Win;
        m_snapshot.result.winReason = WinReason::BossDefeated;
        m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
        m_snapshot.screenMessage.clear();
    } else {
        m_stageIndex = std::max<std::uint32_t>(m_stageIndex, 1);
        m_snapshot.phase = GamePhase::ClearToGo;
        m_snapshot.screenMessage.clear();
    }
}

bool GameSimulation::is_gameplay_active() const noexcept
{
    return m_snapshot.phase == GamePhase::Playing ||
           m_snapshot.phase == GamePhase::EncounterLocked ||
           m_snapshot.phase == GamePhase::ClearToGo;
}

bool GameSimulation::is_player_action_locked() const noexcept
{
    return m_playerActionTimer > 0.0f && is_attack_state(m_snapshot.player.state);
}

// 矩形相交判定，用于攻击盒和身体盒的碰撞检测。
bool GameSimulation::rects_intersect(const Rect& a, const Rect& b) noexcept
{
    return !(a.x + a.width < b.x || b.x + b.width < a.x || a.y + a.height < b.y || b.y + b.height < a.y);
}

// 身体盒不放进公开快照，模拟层按当前位置和绘制尺寸即时计算。
Rect GameSimulation::actor_body_rect(const ActorSnapshot& actor) const noexcept
{
    return {
        actor.position.x - actor.drawSize.width / 2.0f,
        actor.position.laneY - actor.drawSize.height,
        actor.drawSize.width,
        actor.drawSize.height
    };
}

// 将攻击盒从角色局部坐标转换到世界坐标。
Rect GameSimulation::combat_box_world_rect(const ActorSnapshot& owner, const CombatBox& box) const noexcept
{
    Rect r;
    float worldX = owner.position.x + box.localBounds.x;
    float worldY = owner.position.laneY + box.localBounds.y;
    r.x = worldX;
    r.y = worldY;
    r.width = box.localBounds.width;
    r.height = box.localBounds.height;
    return r;
}

} // namespace alleyfist
