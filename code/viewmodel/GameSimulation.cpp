#include "GameSimulation.h"

#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace alleyfist {

GameSimulation::GameSimulation()
{
    reset();
}

void GameSimulation::reset()
{
    m_snapshot = {};
    m_snapshot.phase = GamePhase::Playing;
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
}

void GameSimulation::step(float deltaSeconds, std::uint64_t frameIndex)
{
    m_frame = frameIndex;
    m_snapshot.frameIndex = frameIndex;
    m_snapshot.elapsedSeconds += deltaSeconds;

    while (!m_commandQueue.empty()) {
        auto cmd = m_commandQueue.front();
        m_commandQueue.pop_front();
        if (cmd.type == CommandType::Input) {
            process_input_command(cmd);
        }
    }

    m_hitThisFrame.clear();
    apply_movement(deltaSeconds);
    update_player(deltaSeconds);
    update_enemies(deltaSeconds);
    apply_attacks();
    check_encounters();

    if (m_snapshot.player.energy.maximum > 0 && !m_snapshot.hud.playerExhausted) {
        m_snapshot.player.energy.current = std::min(m_snapshot.player.energy.maximum,
            static_cast<int>(m_snapshot.player.energy.current + m_rules.energyRecoveryPerSecond * deltaSeconds));
    }
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
    if (command.type == CommandType::Restart) {
        reset();
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

    m_snapshot.player.bodyBox = {m_snapshot.player.position.x - 32.0f, m_snapshot.player.position.laneY - 96.0f, 64.0f, 96.0f};
    m_snapshot.hud.playerHealth = m_snapshot.player.health;
    m_snapshot.hud.playerEnergy = m_snapshot.player.energy;

    if (m_snapshot.hud.comboTimeLeftSeconds > 0.0f) {
        m_snapshot.hud.comboTimeLeftSeconds = std::max(0.0f, m_snapshot.hud.comboTimeLeftSeconds - dt);
        if (m_snapshot.hud.comboTimeLeftSeconds <= 0.0f) {
            m_snapshot.hud.comboStep = 0;
        }
    }

    if (m_snapshot.player.position.z > 0.0f) {
        m_jumpElapsed += dt;
        float progress = std::min(1.0f, m_jumpElapsed / m_rules.jumpSeconds);
        m_snapshot.player.position.z = m_rules.jumpHeight * (1.0f - std::pow(1.0f - progress, 2.0f));
        if (progress >= 1.0f) {
            m_jumpElapsed = 0.0f;
            m_jumpActive = false;
            m_snapshot.player.position.z = 0.0f;
            m_snapshot.player.state = ActorState::Idle;
        }
    }
}

void GameSimulation::update_enemies(float dt)
{
    std::vector<ActorViewData> alive;
    for (auto& e : m_snapshot.enemies) {
        if (e.state == ActorState::Dead) continue;
        if (e.state != ActorState::Hurt && e.state != ActorState::KnockedDown) {
            float dir = (m_snapshot.player.position.x < e.position.x) ? -1.0f : 1.0f;
            e.position.x += dir * 30.0f * dt;
            e.position.laneY += (m_snapshot.player.position.laneY > e.position.laneY) ? 12.0f * dt : -12.0f * dt;
        }
        auto& t = m_enemyAttackTimers[e.id];
        t -= dt;
        if (t <= 0.0f) {
            float dx = std::abs(e.position.x - m_snapshot.player.position.x);
            if (dx < 40.0f) {
                m_snapshot.player.health.current -= 8;
                m_snapshot.player.state = ActorState::Hurt;
                t = 1.0f;
                if (m_snapshot.player.health.current <= 0) {
                    m_snapshot.player.state = ActorState::Dead;
                    m_snapshot.phase = GamePhase::GameOver;
                }
            }
        }
        if (e.position.x < 0.0f) e.position.x = 0.0f;
        if (e.position.x > m_snapshot.map.worldWidth) e.position.x = m_snapshot.map.worldWidth;
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

        if (fromAir) {
            m_snapshot.player.state = ActorState::AirAttack;
            m_snapshot.player.position.z = m_rules.jumpHeight * 0.5f;
        } else if (kind == AttackKind::LightPunch) {
            m_snapshot.player.state = ActorState::LightAttack;
        } else if (kind == AttackKind::HeavyStrike) {
            m_snapshot.player.state = ActorState::HeavyAttack;
        }

        CombatBoxViewData cb;
        cb.ownerId = m_snapshot.player.id;
        if (kind == AttackKind::LightPunch) {
            cb.damage = 20;
            cb.attack = AttackKind::LightPunch;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 20.0f : -80.0f, -60.0f, 60.0f, 40.0f };
        } else if (kind == AttackKind::HeavyStrike) {
            cb.damage = 40;
            cb.attack = AttackKind::HeavyStrike;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 30.0f : -110.0f, -70.0f, 80.0f, 60.0f };
        } else if (fromAir) {
            cb.damage = 30;
            cb.attack = AttackKind::JumpKick;
            cb.localBounds = { (m_snapshot.player.facing == Facing::Right) ? 20.0f : -90.0f, -40.0f, 70.0f, 50.0f };
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
                e.health.current -= cb.damage;
                if (e.health.current <= 0) {
                    e.state = ActorState::Dead;
                    e.visible = false;
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

        if (!fromAir) {
            m_snapshot.player.state = ActorState::Idle;
        }
    }
}

void GameSimulation::process_input_command(const GameCommand& command)
{
    const auto action = command.input.action;
    const auto state = command.input.state;

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
        if (!m_snapshot.hud.playerExhausted) {
            if (m_snapshot.player.position.z > 0.0f) {
                m_pendingAirAttack = true;
                m_pendingAttackKind = AttackKind::JumpKick;
            } else {
                m_pendingAttack = true;
                m_pendingAttackKind = AttackKind::LightPunch;
            }
        }
    } else if (action == InputAction::HeavyAttack && state == ButtonState::Triggered) {
        if (!m_snapshot.hud.playerExhausted) {
            m_pendingAttack = true;
            m_pendingAttackKind = AttackKind::HeavyStrike;
        }
    } else if (action == InputAction::Restart && state == ButtonState::Triggered) {
        reset();
    }
}

void GameSimulation::apply_movement(float dt)
{
    if (m_snapshot.player.state == ActorState::Dead || m_snapshot.phase == GamePhase::GameOver)
        return;

    float vx = 0.0f;
    float vy = 0.0f;
    if (m_moveLeft) vx -= m_rules.playerMoveSpeed * dt;
    if (m_moveRight) vx += m_rules.playerMoveSpeed * dt;
    if (m_moveUp) vy -= m_rules.playerMoveSpeed * 0.6f * dt;
    if (m_moveDown) vy += m_rules.playerMoveSpeed * 0.6f * dt;

    if (std::abs(vx) > 0.0f || std::abs(vy) > 0.0f) {
        m_snapshot.player.position.x += vx;
        m_snapshot.player.position.laneY += vy;
        m_snapshot.player.state = (m_snapshot.player.position.z > 0.0f) ? ActorState::Jump : ActorState::Walk;
        m_snapshot.player.facing = (vx < 0.0f) ? Facing::Left : Facing::Right;
    } else if (m_snapshot.player.position.z <= 0.0f) {
        m_snapshot.player.state = ActorState::Idle;
    }

    if (m_snapshot.player.position.x < 0.0f) m_snapshot.player.position.x = 0.0f;
    if (m_snapshot.player.position.x > m_snapshot.map.worldWidth) m_snapshot.player.position.x = m_snapshot.map.worldWidth;
    if (m_snapshot.player.position.laneY < m_snapshot.map.streetTopY) m_snapshot.player.position.laneY = m_snapshot.map.streetTopY;
    if (m_snapshot.player.position.laneY > m_snapshot.map.streetBottomY) m_snapshot.player.position.laneY = m_snapshot.map.streetBottomY;
}

void GameSimulation::begin_jump()
{
    m_jumpActive = true;
    m_snapshot.player.position.z = 0.0f;
    m_snapshot.player.state = ActorState::Jump;
    m_jumpElapsed = 0.0f;
}

void GameSimulation::check_encounters()
{
    // encounter trigger at specific X positions
    // if player reaches a trigger and there are no active encounters, spawn based on simple rule
    static const float kTriggerX = 400.0f;
    if (m_snapshot.player.position.x > kTriggerX && m_snapshot.progress.activeEncounterId == kInvalidEncounterId) {
        if (m_snapshot.enemies.empty()) {
            // spawn encounter and lock scrolling
            m_snapshot.progress.activeEncounterId = 1;
            m_snapshot.map.scrollLock = ScrollLockState::LockedByEncounter;
            m_snapshot.map.showGoIndicator = false;
            EncounterViewData enc;
            enc.id = 1;
            enc.state = EncounterState::Triggered;
            enc.triggerX = kTriggerX;
            enc.bossEncounter = false;
            enc.spawnedCount = 3;
            enc.remainingCount = 3;
            m_snapshot.map.encounters.push_back(enc);

            for (int i = 0; i < 3; ++i) {
                ActorViewData e;
                e.id = static_cast<ActorId>(100 + i);
                e.kind = ActorKind::Grunt;
                e.team = Team::Enemy;
                e.position.x = m_snapshot.player.position.x + 200.0f + i * 40.0f;
                e.position.laneY = m_snapshot.player.position.laneY + ((i % 2 == 0) ? -20.0f : 20.0f);
                e.health = {30, 30};
                e.state = ActorState::Spawn;
                e.visible = true;
                e.drawSize = {48.0f, 72.0f};
                e.bodyBox = {e.position.x - 24.0f, e.position.laneY - 72.0f, 48.0f, 72.0f};
                e.debugName = "Grunt";
                m_snapshot.enemies.push_back(e);
                m_enemyAttackTimers[e.id] = 0.5f + i * 0.2f;
            }
        }
    }

    // check clear
    if (!m_snapshot.enemies.empty()) {
        bool anyAlive = false;
        for (auto& e : m_snapshot.enemies) {
            if (e.state != ActorState::Dead) { anyAlive = true; break; }
        }
        if (!anyAlive) {
            // cleared encounter
            m_snapshot.map.scrollLock = ScrollLockState::Free;
            m_snapshot.map.showGoIndicator = true;
            // update encounter view data
            for (auto& enc : m_snapshot.map.encounters) {
                if (enc.id == m_snapshot.progress.activeEncounterId) {
                    enc.state = EncounterState::Cleared;
                    enc.remainingCount = 0;
                }
            }
            m_snapshot.progress.activeEncounterId = kInvalidEncounterId;
        }
    } else {
        // no enemies present
        m_snapshot.map.showGoIndicator = true;
    }
}

bool GameSimulation::rects_intersect(const Rect& a, const Rect& b) noexcept
{
    return !(a.x + a.width < b.x || b.x + b.width < a.x || a.y + a.height < b.y || b.y + b.height < a.y);
}

Rect GameSimulation::combat_box_world_rect(const ActorViewData& owner, const CombatBoxViewData& box) const noexcept
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
