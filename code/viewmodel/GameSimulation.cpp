#include "GameSimulation.h"

#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace alleyfist {

// 所有玩法规则集中在 Simulation：移动、攻击、敌人、遭遇战、胜负判定都在这里。
// View 层只接收最终快照，因此规则变化不会反向污染绘制层。

namespace {

constexpr float kPi = 3.1415926535f;
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
    m_snapshot.player.id = kPlayerActorId;
    m_snapshot.player.kind = ActorKind::Player;
    m_snapshot.player.team = Team::Player;
    m_snapshot.player.position.x = 50.0f;
    m_snapshot.player.position.laneY = 400.0f;
    m_snapshot.player.position.z = 0.0f;
    m_snapshot.player.health = {100, 100};
    m_snapshot.player.energy = {100, 100};
    m_snapshot.player.state = ActorState::Idle;
    m_snapshot.player.drawSize = {64.0f, 96.0f};
    m_snapshot.player.bodyBox = {m_snapshot.player.position.x - 32.0f, m_snapshot.player.position.laneY - 96.0f, 64.0f, 96.0f};
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
    m_hitThisFrame.clear();
    m_rules = GameRules();
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

    m_snapshot.elapsedSeconds += deltaSeconds;
    m_hitThisFrame.clear();
    apply_movement(deltaSeconds);
    update_player(deltaSeconds);
    update_enemies(deltaSeconds);
    apply_attacks();
    check_encounters();

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
    if (m_snapshot.player.position.x < 0.0f)
        m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth)
        m_snapshot.player.position.x = m_snapshot.map.worldWidth;

    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;

    update_actor_body_box(m_snapshot.player);
    m_snapshot.hud.playerHealth = m_snapshot.player.health;
    m_snapshot.hud.playerEnergy = m_snapshot.player.energy;

    if (m_snapshot.hud.comboTimeLeftSeconds > 0.0f) {
        m_snapshot.hud.comboTimeLeftSeconds = std::max(0.0f, m_snapshot.hud.comboTimeLeftSeconds - dt);
        if (m_snapshot.hud.comboTimeLeftSeconds <= 0.0f) {
            m_snapshot.hud.comboStep = 0;
        }
    }

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

    if (m_playerActionTimer > 0.0f) {
        m_playerActionTimer = std::max(0.0f, m_playerActionTimer - dt);
        if (m_playerActionTimer <= 0.0f && is_attack_state(m_snapshot.player.state)) {
            m_snapshot.player.state = m_jumpActive ? ActorState::Jump : ActorState::Idle;
        }
    }

    update_actor_body_box(m_snapshot.player);
}

void GameSimulation::update_enemies(float dt)
{
    std::vector<ActorSnapshot> alive;
    for (auto& e : m_snapshot.enemies) {
        if (e.state == ActorState::Dead || !e.visible || e.health.current <= 0) {
            continue;
        }
        if (e.state != ActorState::Hurt && e.state != ActorState::KnockedDown) {
            float dir = (m_snapshot.player.position.x < e.position.x) ? -1.0f : 1.0f;
            const float speed = (e.kind == ActorKind::Boss) ? m_rules.bossMoveSpeed : m_rules.enemyMoveSpeed;
            e.position.x += dir * speed * dt;
            e.position.laneY += (m_snapshot.player.position.laneY > e.position.laneY) ? 45.0f * dt : -45.0f * dt;
        }
        auto& t = m_enemyAttackTimers[e.id];
        t -= dt;
        if (t <= 0.0f) {
            float dx = std::abs(e.position.x - m_snapshot.player.position.x);
            float dy = std::abs(e.position.laneY - m_snapshot.player.position.laneY);
            if (dx < 48.0f && dy < 42.0f && m_snapshot.player.position.z < 35.0f) {
                const int damage = (e.kind == ActorKind::Boss) ? 16 : 8;
                m_snapshot.player.health.current = std::max(0, m_snapshot.player.health.current - damage);
                m_snapshot.player.state = ActorState::Hurt;
                t = 1.0f;
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
        update_actor_body_box(e);
        if (e.state == ActorState::Hurt || e.state == ActorState::Spawn) {
            e.state = ActorState::Idle;
        }
        alive.push_back(e);
    }
    m_snapshot.enemies = std::move(alive);
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
        cb.ownerId = m_snapshot.player.id;
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

        Rect attackRect = combat_box_world_rect(m_snapshot.player, cb);
        for (auto& e : m_snapshot.enemies) {
            if (!e.visible) continue;
            if (m_hitThisFrame.find(e.id) != m_hitThisFrame.end()) continue;
            if (e.team == m_snapshot.player.team) continue;
            Rect body = e.bodyBox;
            if (body.is_empty()) {
                body.x = e.position.x - e.drawSize.width / 2.0f;
                body.y = e.position.laneY - e.drawSize.height;
                body.width = e.drawSize.width;
                body.height = e.drawSize.height;
            }
            if (rects_intersect(attackRect, body)) {
                e.health.current = std::max(0, e.health.current - cb.damage);
                if (e.health.current <= 0) {
                    e.state = ActorState::Dead;
                    e.visible = false;
                    e.targetable = false;
                    ++m_snapshot.result.defeatedEnemies;
                    if (e.kind == ActorKind::Boss) {
                        m_snapshot.progress.bossDefeated = true;
                    }
                } else {
                    e.state = ActorState::Hurt;
                }
                m_hitThisFrame.insert(e.id);
                m_snapshot.hud.comboStep = std::min(3u, m_snapshot.hud.comboStep + 1u);
                m_snapshot.hud.comboTimeLeftSeconds = m_comboWindowSeconds;
            }
        }

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
    if (!is_player_action_locked() || m_jumpActive) {
        if (m_moveLeft) vx -= m_rules.playerMoveSpeed * dt;
        if (m_moveRight) vx += m_rules.playerMoveSpeed * dt;
        if (m_moveUp) vy -= m_rules.playerMoveSpeed * 0.6f * dt;
        if (m_moveDown) vy += m_rules.playerMoveSpeed * 0.6f * dt;
    }

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

    if (m_snapshot.player.position.x < 0.0f) m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth) m_snapshot.player.position.x = m_snapshot.map.worldWidth;
    if (m_snapshot.map.scrollLock == ScrollLockState::LockedByEncounter ||
        m_snapshot.map.scrollLock == ScrollLockState::LockedByBoss) {
        m_snapshot.player.position.x = std::clamp(m_snapshot.player.position.x,
                                                  m_snapshot.map.leftBoundaryX,
                                                  m_snapshot.map.rightBoundaryX);
    }
    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY) m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY) m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;
}

void GameSimulation::begin_jump()
{
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

    if (m_snapshot.progress.activeEncounterId != kInvalidEncounterId) {
        const bool anyAlive = std::any_of(m_snapshot.enemies.begin(), m_snapshot.enemies.end(),
            [](const ActorSnapshot& actor) {
                return actor.team == Team::Enemy && actor.visible && actor.health.current > 0;
            });
        if (!anyAlive) {
            clear_active_encounter();
        }
        return;
    }

    if (m_snapshot.progress.stageIndex == 0 && m_snapshot.player.position.x >= kGruntTriggerX) {
        spawn_grunt_encounter();
        return;
    }

    if (!m_snapshot.progress.bossSpawned && m_snapshot.player.position.x >= kBossTriggerX) {
        spawn_boss_encounter();
        return;
    }

    if (m_snapshot.progress.bossDefeated &&
        m_snapshot.player.position.x >= m_snapshot.map.worldWidth - 120.0f) {
        m_snapshot.phase = GamePhase::Win;
        m_snapshot.map.scrollLock = ScrollLockState::LevelFinished;
        m_snapshot.result.winReason = WinReason::BossDefeated;
        m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
        m_snapshot.screenMessage.clear();
    }
}

void GameSimulation::update_camera()
{
    const float maxCamera = std::max(0.0f, m_snapshot.map.worldWidth - m_snapshot.map.viewportWidth);
    float desired = m_snapshot.player.position.x - m_snapshot.map.viewportWidth * 0.42f;

    if (m_snapshot.map.scrollLock == ScrollLockState::LockedByEncounter ||
        m_snapshot.map.scrollLock == ScrollLockState::LockedByBoss) {
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
    if (m_snapshot.map.worldWidth > 0.0f) {
        m_snapshot.progress.progressRatio = std::clamp(m_snapshot.player.position.x / m_snapshot.map.worldWidth,
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

void GameSimulation::update_actor_body_box(ActorSnapshot& actor) const noexcept
{
    actor.bodyBox = {
        actor.position.x - actor.drawSize.width / 2.0f,
        actor.position.laneY - actor.drawSize.height,
        actor.drawSize.width,
        actor.drawSize.height
    };
    actor.depthSortY = actor.position.laneY;
}

void GameSimulation::spawn_grunt_encounter()
{
    m_snapshot.progress.activeEncounterId = 1;
    m_snapshot.phase = GamePhase::EncounterLocked;
    m_snapshot.map.scrollLock = ScrollLockState::LockedByEncounter;
    m_snapshot.map.showGoIndicator = false;
    m_snapshot.map.leftBoundaryX = std::max(0.0f, m_snapshot.player.position.x - kEncounterLeftPadding);
    m_snapshot.map.rightBoundaryX = std::min(m_snapshot.map.worldWidth, m_snapshot.player.position.x + kEncounterRightPadding);
    m_snapshot.screenMessage.clear();

    EncounterSnapshot enc;
    enc.id = 1;
    enc.state = EncounterState::Locked;
    enc.lockState = ScrollLockState::LockedByEncounter;
    enc.triggerX = kGruntTriggerX;
    enc.bossEncounter = false;
    enc.spawnedCount = 3;
    enc.remainingCount = 3;
    m_snapshot.map.encounters.push_back(enc);

    for (int i = 0; i < 3; ++i) {
        ActorSnapshot enemy;
        enemy.id = static_cast<ActorId>(100 + i);
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
        update_actor_body_box(enemy);
        m_snapshot.enemies.push_back(enemy);
        m_enemyAttackTimers[enemy.id] = 0.8f + i * 0.2f;
    }
}

void GameSimulation::spawn_boss_encounter()
{
    m_snapshot.progress.activeEncounterId = 2;
    m_snapshot.progress.bossSpawned = true;
    m_snapshot.phase = GamePhase::EncounterLocked;
    m_snapshot.map.scrollLock = ScrollLockState::LockedByBoss;
    m_snapshot.map.showGoIndicator = false;
    m_snapshot.map.leftBoundaryX = std::max(0.0f, m_snapshot.player.position.x - kEncounterLeftPadding);
    m_snapshot.map.rightBoundaryX = std::min(m_snapshot.map.worldWidth, m_snapshot.player.position.x + kEncounterRightPadding);
    m_snapshot.screenMessage.clear();

    EncounterSnapshot enc;
    enc.id = 2;
    enc.state = EncounterState::Locked;
    enc.lockState = ScrollLockState::LockedByBoss;
    enc.triggerX = kBossTriggerX;
    enc.bossEncounter = true;
    enc.spawnedCount = 1;
    enc.remainingCount = 1;
    m_snapshot.map.encounters.push_back(enc);

    ActorSnapshot boss;
    boss.id = 900;
    boss.kind = ActorKind::Boss;
    boss.team = Team::Enemy;
    boss.position.x = std::min(m_snapshot.map.worldWidth - 120.0f, m_snapshot.player.position.x + 220.0f);
    boss.position.laneY = m_snapshot.player.position.laneY;
    boss.health = {140, 140};
    boss.state = ActorState::Spawn;
    boss.visible = true;
    boss.drawSize = {82.0f, 124.0f};
    update_actor_body_box(boss);
    m_snapshot.enemies.push_back(boss);
    m_enemyAttackTimers[boss.id] = 1.0f;
    update_progress();
}

void GameSimulation::clear_active_encounter()
{
    const EncounterId clearedId = m_snapshot.progress.activeEncounterId;
    const bool bossCleared = std::any_of(m_snapshot.map.encounters.begin(),
        m_snapshot.map.encounters.end(),
        [clearedId](const EncounterSnapshot& enc) {
            return enc.id == clearedId && enc.bossEncounter;
        });

    for (auto& enc : m_snapshot.map.encounters) {
        if (enc.id == clearedId) {
            enc.state = EncounterState::Cleared;
            enc.lockState = ScrollLockState::Free;
            enc.defeatedCount = enc.spawnedCount;
            enc.remainingCount = 0;
        }
    }

    m_snapshot.progress.activeEncounterId = kInvalidEncounterId;
    m_snapshot.map.scrollLock = ScrollLockState::Free;
    m_snapshot.map.leftBoundaryX = 0.0f;
    m_snapshot.map.rightBoundaryX = m_snapshot.map.worldWidth;
    m_snapshot.map.showGoIndicator = true;

    if (bossCleared) {
        m_snapshot.progress.stageIndex = 2;
        m_snapshot.progress.bossDefeated = true;
        m_snapshot.hud.showBossHealth = false;
        m_snapshot.phase = GamePhase::Win;
        m_snapshot.result.winReason = WinReason::BossDefeated;
        m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
        m_snapshot.screenMessage.clear();
    } else {
        m_snapshot.progress.stageIndex = std::max<std::uint32_t>(m_snapshot.progress.stageIndex, 1);
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

bool GameSimulation::rects_intersect(const Rect& a, const Rect& b) noexcept
{
    return !(a.x + a.width < b.x || b.x + b.width < a.x || a.y + a.height < b.y || b.y + b.height < a.y);
}

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
