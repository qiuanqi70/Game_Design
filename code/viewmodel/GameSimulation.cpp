#include "GameSimulation.h"

#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace alleyfist {

// 所有玩法规则集中在 Simulation：移动、攻击、敌人、遭遇战、胜负判定都在这里。
// View 层只接收最终快照，因此规则变化不会反向污染绘制层。

namespace {

//设置一系列关于游戏的常量，例如 PI、敌人触发位置、攻击持续时间等
constexpr float kPi = 3.1415926535f;
constexpr float kGruntTriggerX = 400.0f; //敌人触发位置，玩家到达这个位置时会触发敌人出现
constexpr float kBossTriggerX = 2350.0f; //Boss触发位置，玩家到达这个位置时会触发Boss出现
constexpr float kEncounterLeftPadding = 240.0f; //遭遇战左边界，玩家到达这个位置时会触发遭遇战
constexpr float kEncounterRightPadding = 360.0f; //遭遇战右边界，玩家到达这个位置时会触发遭遇战
constexpr float kLightAttackSeconds = 0.18f; //轻攻击持续时间，表示轻攻击动作的动画和效果持续的时间
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

//推进游戏状态，处理 Tick 命令
void GameSimulation::step(float deltaSeconds, std::uint64_t frameIndex)
{
    m_frame = frameIndex;
    m_snapshot.frameIndex = frameIndex;

    //1. 处理命令队列中的所有输入命令，调用 process_input_command() 方法处理每个命令
    while (!m_commandQueue.empty()) {
        auto cmd = m_commandQueue.front();
        m_commandQueue.pop_front();
        if (cmd.type == CommandType::Input) {
            process_input_command(cmd); //处理input命令，处理玩家输入
        }
    }

    if (!is_gameplay_active()) { //如果游戏不处于活动状态（例如暂停或游戏结束），则不进行游戏逻辑更新
        //update_camera();
        //update_progress();
        return;
    }

    //2. 累计时间，用于处理游戏逻辑的时间步长同时实现移动、攻击、敌人、遭遇战、胜负判定等游戏逻辑
    m_snapshot.elapsedSeconds += deltaSeconds; //累计时间，用于处理游戏逻辑的时间步长
    m_hitThisFrame.clear(); //清空当前帧中已经被攻击过的敌人记录，避免同一攻击动作对同一敌人造成多次伤害
    apply_movement(deltaSeconds);
    update_player(deltaSeconds);
    update_enemies(deltaSeconds);
    apply_attacks();
    check_encounters();

    //3. 能量恢复逻辑，如果玩家的能量条未满，则进行能量恢复
    if (m_snapshot.player.energy.maximum > 0 &&
        m_snapshot.player.energy.current < m_snapshot.player.energy.maximum) { //如果玩家的能量条未满，则进行能量恢复
        m_energyRecoveryCarry += m_rules.energyRecoveryPerSecond * deltaSeconds;
        const int recovered = static_cast<int>(m_energyRecoveryCarry); //计算恢复的能量值，取整
        if (recovered > 0) {
            m_snapshot.player.energy.current = std::min(m_snapshot.player.energy.maximum,
                                                        m_snapshot.player.energy.current + recovered);
            m_energyRecoveryCarry -= static_cast<float>(recovered);
        }
        if (m_snapshot.player.energy.current >= m_rules.lightAttackEnergyCost) { //如果玩家的能量条恢复到足够进行轻攻击的能量值，则取消疲劳状态
            m_snapshot.hud.playerExhausted = false;
        }
    } else if (m_snapshot.player.energy.current >= m_snapshot.player.energy.maximum) { //如果玩家的能量条已满，则重置能量恢复携带值，并取消疲劳状态
        m_energyRecoveryCarry = 0.0f;
        m_snapshot.hud.playerExhausted = false;
    }

    //4. 更新 HUD 中的玩家血量和能量条，确保 HUD 显示的数值与玩家实际状态一致
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
    //玩家位置限制，确保玩家不会超出游戏世界的边界
    if (m_snapshot.player.position.x < 0.0f)
        m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth)
        m_snapshot.player.position.x = m_snapshot.map.worldWidth;

    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY)
        m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;

    //更新玩家的身体碰撞盒，确保碰撞检测的准确性
    update_actor_body_box(m_snapshot.player);
    m_snapshot.hud.playerHealth = m_snapshot.player.health;
    m_snapshot.hud.playerEnergy = m_snapshot.player.energy;

    //更新连击计时器，如果连击时间剩余大于0，则减少连击时间，并在时间耗尽时重置连击步数
    if (m_snapshot.hud.comboTimeLeftSeconds > 0.0f) {
        m_snapshot.hud.comboTimeLeftSeconds = std::max(0.0f, m_snapshot.hud.comboTimeLeftSeconds - dt);
        if (m_snapshot.hud.comboTimeLeftSeconds <= 0.0f) {
            m_snapshot.hud.comboStep = 0;
        }
    }

    //处理玩家跳跃逻辑，如果玩家正在跳跃，则根据跳跃时间计算玩家的垂直位置，并在跳跃结束时重置状态
    if (m_jumpActive) {
        m_jumpElapsed += dt; //累计跳跃时间，用于计算跳跃进度
        const float progress = std::min(1.0f, m_jumpElapsed / m_rules.jumpSeconds); //计算跳跃进度，确保进度不会超过1.0
        m_snapshot.player.position.z = m_rules.jumpHeight * std::sin(kPi * progress); //根据跳跃进度计算玩家的垂直位置，使用正弦函数实现跳跃的抛物线效果
        if (progress >= 1.0f) { //如果跳跃进度达到1.0，表示跳跃结束，重置跳跃状态和垂直位置
            m_jumpElapsed = 0.0f;
            m_jumpActive = false;
            m_snapshot.player.position.z = 0.0f;
            if (m_playerActionTimer <= 0.0f || !is_attack_state(m_snapshot.player.state)) { //如果玩家的动作计时器已耗尽，且玩家当前不处于攻击状态，则将玩家状态重置为Idle
                m_snapshot.player.state = ActorState::Idle;
            }
        } else if (!is_attack_state(m_snapshot.player.state)) {
            m_snapshot.player.state = ActorState::Jump;
        }
    }

    //处理玩家动作计时器，如果计时器大于0，则减少计时器，并在计时器耗尽时根据玩家当前状态进行相应的状态更新
    if (m_playerActionTimer > 0.0f) {
        m_playerActionTimer = std::max(0.0f, m_playerActionTimer - dt);
        if (m_playerActionTimer <= 0.0f && is_attack_state(m_snapshot.player.state)) {
            m_snapshot.player.state = m_jumpActive ? ActorState::Jump : ActorState::Idle;
        }
    }

    //更新玩家的身体碰撞盒，确保碰撞检测的准确性
    update_actor_body_box(m_snapshot.player);
}

void GameSimulation::update_enemies(float dt)
{
    std::vector<ActorSnapshot> alive;
    for (auto& e : m_snapshot.enemies) {
        if (e.state == ActorState::Dead || !e.visible || e.health.current <= 0) { //如果敌人已经死亡、不可见或血量为0，则跳过该敌人的更新
            continue;
        }
        if (e.state != ActorState::Hurt && e.state != ActorState::KnockedDown) { //如果敌人当前不处于受伤或击倒状态，则进行移动逻辑
            float dir = (m_snapshot.player.position.x < e.position.x) ? -1.0f : 1.0f;
            const float speed = (e.kind == ActorKind::Boss) ? m_rules.bossMoveSpeed : m_rules.enemyMoveSpeed;
            e.position.x += dir * speed * dt;
            e.position.laneY += (m_snapshot.player.position.laneY > e.position.laneY) ? 45.0f * dt : -45.0f * dt;
        }
        auto& t = m_enemyAttackTimers[e.id]; //敌人的攻击冷却时间
        t -= dt;
        if (t <= 0.0f) { //如果敌人的攻击冷却时间已耗尽，则进行攻击逻辑
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
        //敌人位置限制，确保敌人不会超出游戏世界的边界
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
        //根据攻击类型设置攻击盒的属性，包括伤害值、攻击类型和攻击盒的局部边界
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

        //计算攻击盒在世界坐标系中的矩形，用于碰撞检测
        Rect attackRect = combat_box_world_rect(m_snapshot.player, cb);
        for (auto& e : m_snapshot.enemies) {
            if (!e.visible) continue;
            if (m_hitThisFrame.find(e.id) != m_hitThisFrame.end()) continue;
            if (e.team == m_snapshot.player.team) continue;
            Rect body = e.bodyBox;
            if (body.is_empty()) { //如果敌人的身体碰撞盒为空（即未初始化），则根据敌人的位置和绘制尺寸计算身体碰撞盒
                body.x = e.position.x - e.drawSize.width / 2.0f;
                body.y = e.position.laneY - e.drawSize.height;
                body.width = e.drawSize.width;
                body.height = e.drawSize.height;
            }
            if (rects_intersect(attackRect, body)) { //如果攻击盒与敌人的身体碰撞盒相交，则表示攻击命中，进行伤害计算和状态更新
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

        //根据攻击类型计算能量消耗，并更新玩家的能量值，如果能量值为0，则设置玩家为疲劳状态
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

    //处理确认和暂停输入，根据当前游戏阶段进行相应的操作，例如重新开始游戏、恢复游戏或暂停游戏
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

    //处理移动和攻击输入，根据输入的动作类型和状态更新玩家的移动方向、跳跃状态和攻击状态
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
    //根据玩家的移动输入状态，计算玩家在水平方向和垂直方向上的速度增量
    if (!is_player_action_locked() || m_jumpActive) {
        if (m_moveLeft) vx -= m_rules.playerMoveSpeed * dt;
        if (m_moveRight) vx += m_rules.playerMoveSpeed * dt;
        if (m_moveUp) vy -= m_rules.playerMoveSpeed * 0.6f * dt;
        if (m_moveDown) vy += m_rules.playerMoveSpeed * 0.6f * dt;
    }

    //根据计算出的速度增量，更新玩家的位置，并根据玩家的移动状态和方向更新玩家的状态和朝向
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

    //限制玩家的位置，确保玩家不会超出游戏世界的边界，并根据滚动锁定状态调整玩家的位置
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

//初始化跳跃状态，设置玩家的垂直位置和状态，并扣除相应的能量值
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

//根据攻击类型和是否来自空中，设置玩家的攻击状态和动作计时器，并在必要时调整玩家的垂直位置
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

//检查当前游戏状态，判断是否需要触发遭遇战或胜利条件，并根据玩家的位置和敌人的状态更新游戏进度
void GameSimulation::check_encounters()
{
    if (!is_gameplay_active()) {
        return;
    }

    if (m_snapshot.progress.activeEncounterId != kInvalidEncounterId) { //如果当前存在活跃的遭遇战，则检查敌人是否仍然存活，如果没有存活的敌人，则清除当前遭遇战
        const bool anyAlive = std::any_of(m_snapshot.enemies.begin(), m_snapshot.enemies.end(),
            [](const ActorSnapshot& actor) {
                return actor.team == Team::Enemy && actor.visible && actor.health.current > 0;
            });
        if (!anyAlive) {
            clear_active_encounter();
        }
        return;
    }

    //根据玩家的位置和游戏进度，判断是否需要触发敌人遭遇战或Boss遭遇战，并在满足条件时调用相应的函数进行处理
    if (m_snapshot.progress.stageIndex == 0 && m_snapshot.player.position.x >= kGruntTriggerX) {
        spawn_grunt_encounter();
        return;
    }

    if (!m_snapshot.progress.bossSpawned && m_snapshot.player.position.x >= kBossTriggerX) {
        spawn_boss_encounter();
        return;
    }

    //根据玩家是否击败了Boss以及玩家的位置，判断是否触发胜利条件，如果满足条件，则更新游戏阶段为胜利，并设置滚动锁定状态和胜利原因
    if (m_snapshot.progress.bossDefeated &&
        m_snapshot.player.position.x >= m_snapshot.map.worldWidth - 120.0f) {
        m_snapshot.phase = GamePhase::Win;
        m_snapshot.map.scrollLock = ScrollLockState::LevelFinished;
        m_snapshot.result.winReason = WinReason::BossDefeated;
        m_snapshot.result.elapsedSeconds = m_snapshot.elapsedSeconds;
        m_snapshot.screenMessage.clear();
    }
}

//更新摄像机位置，根据玩家的位置和滚动锁定状态计算摄像机的目标位置，并限制摄像机在游戏世界的边界内
void GameSimulation::update_camera()
{
    const float maxCamera = std::max(0.0f, m_snapshot.map.worldWidth - m_snapshot.map.viewportWidth);
    float desired = m_snapshot.player.position.x - m_snapshot.map.viewportWidth * 0.42f;

    //根据滚动锁定状态调整摄像机位置，如果当前滚动锁定状态为遭遇战或Boss遭遇战，则限制摄像机在遭遇战的边界范围内
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
    if (m_snapshot.map.worldWidth > 0.0f) { //如果游戏世界的宽度大于0，则根据玩家的位置计算游戏进度的比例，并将其限制在0.0到1.0之间（游戏进度的比例表示玩家在游戏世界中的位置相对于整个游戏世界的宽度的百分比）
        m_snapshot.progress.progressRatio = std::clamp(m_snapshot.player.position.x / m_snapshot.map.worldWidth,
                                                       0.0f,
                                                       1.0f);
    }

    m_snapshot.hud.showBossHealth = false;
    for (const auto& enemy : m_snapshot.enemies) { //遍历所有敌人，检查是否存在Boss敌人，如果存在且可见且血量大于0，则显示Boss血量条，并更新HUD中的Boss血量信息
        if (enemy.kind == ActorKind::Boss && enemy.visible && enemy.health.maximum > 0) {
            m_snapshot.hud.showBossHealth = true;
            m_snapshot.hud.bossHealth = enemy.health;
            break;
        }
    }
}

//更新角色的身体碰撞盒，根据角色的位置和绘制尺寸计算身体碰撞盒的坐标和大小，并设置深度排序的Y坐标
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

//生成敌人遭遇战，设置遭遇战的状态、锁定状态和触发位置，并在游戏快照中添加敌人信息
void GameSimulation::spawn_grunt_encounter()
{
    //设置当前活跃的遭遇战ID为1，表示正在进行敌人遭遇战
    m_snapshot.progress.activeEncounterId = 1;
    m_snapshot.phase = GamePhase::EncounterLocked;
    m_snapshot.map.scrollLock = ScrollLockState::LockedByEncounter;
    m_snapshot.map.showGoIndicator = false;
    m_snapshot.map.leftBoundaryX = std::max(0.0f, m_snapshot.player.position.x - kEncounterLeftPadding);
    m_snapshot.map.rightBoundaryX = std::min(m_snapshot.map.worldWidth, m_snapshot.player.position.x + kEncounterRightPadding);
    m_snapshot.screenMessage.clear();

    //创建一个遭遇战数据对象，设置遭遇战的ID、状态、锁定状态、触发位置、是否为Boss遭遇战以及敌人的数量，并将其添加到游戏快照的遭遇战列表中
    EncounterViewData enc;
    enc.id = 1;
    enc.state = EncounterState::Locked;
    enc.lockState = ScrollLockState::LockedByEncounter;
    enc.triggerX = kGruntTriggerX;
    enc.bossEncounter = false;
    enc.spawnedCount = 3;
    enc.remainingCount = 3;
    m_snapshot.map.encounters.push_back(enc);

    //生成三个敌人Grunt，设置敌人的属性，包括ID、种类、队伍、位置、血量、状态、可见性和绘制尺寸，并将其添加到游戏快照的敌人列表中，同时为每个敌人设置攻击冷却时间
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

//生成Boss遭遇战，设置遭遇战的状态、锁定状态和触发位置，并在游戏快照中添加Boss信息
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

    EncounterViewData enc;
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

//清除当前活跃的遭遇战，更新遭遇战的状态、锁定状态和敌人的数量，并根据是否击败Boss来更新游戏阶段和胜利条件
void GameSimulation::clear_active_encounter()
{
    const EncounterId clearedId = m_snapshot.progress.activeEncounterId;
    const bool bossCleared = std::any_of(m_snapshot.map.encounters.begin(),
        m_snapshot.map.encounters.end(),
        [clearedId](const EncounterViewData& enc) {
            return enc.id == clearedId && enc.bossEncounter;
        });

    for (auto& enc : m_snapshot.map.encounters) {
        if (enc.id == clearedId) { //如果当前遭遇战的ID与清除的遭遇战ID匹配，则更新该遭遇战的状态、锁定状态和敌人的数量
            enc.state = EncounterState::Cleared;
            enc.lockState = ScrollLockState::Free;
            enc.defeatedCount = enc.spawnedCount;
            enc.remainingCount = 0;
        }
    }

    //清除当前活跃的遭遇战ID，设置滚动锁定状态为自由，并重置地图的边界和显示状态
    m_snapshot.progress.activeEncounterId = kInvalidEncounterId;
    m_snapshot.map.scrollLock = ScrollLockState::Free;
    m_snapshot.map.leftBoundaryX = 0.0f;
    m_snapshot.map.rightBoundaryX = m_snapshot.map.worldWidth;
    m_snapshot.map.showGoIndicator = true;

    if (bossCleared) { //如果击败了Boss，则更新游戏阶段为胜利，并设置滚动锁定状态和胜利原因
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

//检查玩家的动作是否被锁定，如果玩家的动作计时器大于0且玩家当前处于攻击状态，则表示玩家的动作被锁定
bool GameSimulation::is_player_action_locked() const noexcept
{
    return m_playerActionTimer > 0.0f && is_attack_state(m_snapshot.player.state);
}

//检查两个矩形是否相交，如果两个矩形在水平或垂直方向上没有重叠，则表示它们不相交，返回false；否则返回true
bool GameSimulation::rects_intersect(const Rect& a, const Rect& b) noexcept
{
    return !(a.x + a.width < b.x || b.x + b.width < a.x || a.y + a.height < b.y || b.y + b.height < a.y);
}

//根据角色的快照和攻击盒，计算攻击盒在世界坐标系中的矩形，用于碰撞检测
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
