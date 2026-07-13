#include "GameViewModel.h"

#include <algorithm>
#include <cmath>

namespace alleyfist::viewmodel {

namespace {

constexpr float kMoveSpeed = 220.0f;
constexpr float kWorldWidth = 3000.0f;
constexpr float kViewportWidth = 960.0f;
constexpr float kViewportHeight = 540.0f;
constexpr float kStreetTop = 300.0f;
constexpr float kStreetBottom = 500.0f;
constexpr float kPi = 3.1415926535f;
constexpr float kJumpSeconds = 0.55f;
constexpr float kJumpHeight = 90.0f;
constexpr float kLightAttackSeconds = 0.18f;
constexpr float kHeavyAttackSeconds = 0.28f;
constexpr float kMaxPlayerEnergy = 100.0f;
constexpr float kEnergyRegenPerSecond = 28.0f;
constexpr float kJumpEnergyCost = 18.0f;
constexpr float kLightAttackEnergyCost = 12.0f;
constexpr float kHeavyAttackEnergyCost = 25.0f;
constexpr float kExhaustedWarningSeconds = 0.65f;

} // namespace

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    m_sim->start();
    sync_state_from_simulation();
}

GameViewModel::~GameViewModel() = default;

const GameSnapshot* GameViewModel::get_game_state() const noexcept
{
    return &m_state;
}

std::function<void(float, std::uint64_t)> GameViewModel::get_tick_command()
{
    return [this](float dt, std::uint64_t frameIndex) {
        tick(dt, frameIndex);
    };
}

std::function<void(bool)> GameViewModel::get_move_left_command()
{
    return make_move_command(m_moveLeft);
}

std::function<void(bool)> GameViewModel::get_move_right_command()
{
    return make_move_command(m_moveRight);
}

std::function<void(bool)> GameViewModel::get_move_up_command()
{
    return make_move_command(m_moveUp);
}

std::function<void(bool)> GameViewModel::get_move_down_command()
{
    return make_move_command(m_moveDown);
}

std::function<void()> GameViewModel::get_light_attack_command()
{
    return [this]() {
        begin_attack(ActorState::LightAttack, kLightAttackSeconds, kLightAttackEnergyCost);
    };
}

std::function<void()> GameViewModel::get_heavy_attack_command()
{
    return [this]() {
        begin_attack(ActorState::HeavyAttack, kHeavyAttackSeconds, kHeavyAttackEnergyCost);
    };
}

std::function<void()> GameViewModel::get_jump_command()
{
    return [this]() {
        if (!is_gameplay_active() || m_jumpActive) {
            return;
        }

        if (!try_spend_energy(kJumpEnergyCost)) {
            sync_state_from_simulation();
            notify_state_changed();
            return;
        }

        m_jumpActive = true;
        m_jumpElapsed = 0.0f;
        sync_state_from_simulation();
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_restart_command()
{
    return [this]() {
        if (m_sim) {
            m_sim->reset();
            m_sim->start();
        }
        m_paused = false;
        reset_actions();
        m_state = {};
        m_state.phase = GamePhase::Playing;
        sync_state_from_simulation();
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_confirm_command()
{
    return [this]() {
        if (m_state.phase == GamePhase::Title || m_state.phase == GamePhase::GameOver || m_state.phase == GamePhase::Win) {
            if (m_sim) {
                m_sim->reset();
                m_sim->start();
            }
            m_paused = false;
            reset_actions();
            m_state = {};
            m_state.phase = GamePhase::Playing;
            sync_state_from_simulation();
        } else if (m_state.phase == GamePhase::Paused) {
            m_paused = false;
            m_state.phase = GamePhase::Playing;
            m_state.screenMessage.clear();
        }
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_pause_command()
{
    return [this]() {
        if (m_state.phase == GamePhase::Title || m_state.phase == GamePhase::GameOver || m_state.phase == GamePhase::Win) {
            return;
        }

        m_paused = !m_paused;
        m_state.phase = m_paused ? GamePhase::Paused : GamePhase::Playing;
        m_state.screenMessage = m_paused ? "Paused" : "";
        notify_state_changed();
    };
}

void GameViewModel::start()
{
    if (m_sim) {
        m_sim->start();
    }
    sync_state_from_simulation();
    notify_state_changed();
}

void GameViewModel::stop()
{
    if (m_sim) {
        m_sim->stop();
    }
}

const EntityList& GameViewModel::entities() const noexcept
{
    return m_sim->entities();
}

void GameViewModel::tick(float dt, std::uint64_t frameIndex)
{
    if (!m_sim || !is_gameplay_active()) {
        return;
    }

    m_state.elapsedSeconds += dt;
    update_action_timers(dt);

    const float dx = (m_moveRight ? kMoveSpeed : 0.0f) - (m_moveLeft ? kMoveSpeed : 0.0f);
    const float dy = (m_moveDown ? kMoveSpeed : 0.0f) - (m_moveUp ? kMoveSpeed : 0.0f);
    if (dx != 0.0f || dy != 0.0f) {
        m_sim->player_move(dx * dt, dy * dt);
    }

    m_sim->step(dt);
    m_frameIndex = frameIndex;
    sync_state_from_simulation();
    notify_state_changed();
}

void GameViewModel::sync_state_from_simulation()
{
    m_state.frameIndex = m_frameIndex;
    m_state.map.worldWidth = kWorldWidth;
    m_state.map.viewportWidth = kViewportWidth;
    m_state.map.viewportHeight = kViewportHeight;
    m_state.map.streetTopY = kStreetTop;
    m_state.map.streetBottomY = kStreetBottom;
    m_state.map.leftBoundaryX = 0.0f;
    m_state.map.rightBoundaryX = kViewportWidth;
    m_state.map.showGoIndicator = false;

    const auto& list = m_sim->entities();
    if (list.empty()) {
        return;
    }

    const auto& player = list.front();
    m_state.player.kind = ActorKind::Player;
    m_state.player.team = Team::Player;
    m_state.player.position = player.pos;
    m_state.player.position.x = std::clamp(m_state.player.position.x, 0.0f, kWorldWidth);
    m_state.player.position.laneY = std::clamp(m_state.player.position.laneY, kStreetTop, kStreetBottom);
    m_state.player.drawSize = {64.0f, 96.0f};
    m_state.player.health = {std::max(0, player.hp), player.maxHp > 0 ? player.maxHp : 100};
    m_state.player.energy = {static_cast<int>(std::lround(m_playerEnergy)), static_cast<int>(kMaxPlayerEnergy)};
    m_state.player.visible = player.alive;
    m_state.player.state = player.alive ? ((m_moveLeft || m_moveRight || m_moveUp || m_moveDown) ? ActorState::Walk : ActorState::Idle)
                                        : ActorState::Dead;
    m_state.player.position.z = 0.0f;
    m_state.player.onGround = true;

    if (player.alive && m_jumpActive) {
        const float progress = std::clamp(m_jumpElapsed / kJumpSeconds, 0.0f, 1.0f);
        m_state.player.position.z = kJumpHeight * std::sin(kPi * progress);
        m_state.player.onGround = false;
        m_state.player.state = ActorState::Jump;
    }

    if (player.alive && m_attackTimer > 0.0f) {
        m_state.player.state = m_jumpActive ? ActorState::AirAttack : m_attackState;
    }

    if (m_moveLeft && !m_moveRight) {
        m_state.player.facing = Facing::Left;
    } else if (m_moveRight && !m_moveLeft) {
        m_state.player.facing = Facing::Right;
    }

    m_state.enemies.clear();
    m_state.hud.showBossHealth = false;
    m_state.hud.bossHealth = {};

    std::uint32_t defeatedEnemies = 0;
    for (std::size_t i = 1; i < list.size(); ++i) {
        const auto& entity = list[i];
        if (!entity.alive) {
            ++defeatedEnemies;
        }

        const bool isBoss = entity.kind == EntityKind::Boss;
        const int fallbackMaxHealth = isBoss ? 140 : 20;

        ActorSnapshot actor;
        actor.kind = isBoss ? ActorKind::Boss : ActorKind::Grunt;
        actor.team = Team::Enemy;
        actor.position = entity.pos;
        actor.position.laneY = std::clamp(actor.position.laneY, kStreetTop, kStreetBottom);
        actor.drawSize = isBoss ? alleyfist::Size{88.0f, 128.0f} : alleyfist::Size{48.0f, 72.0f};
        actor.health = {std::max(0, entity.hp), entity.maxHp > 0 ? entity.maxHp : fallbackMaxHealth};
        actor.state = entity.alive ? ActorState::Walk : ActorState::Dead;
        actor.visible = entity.alive;
        actor.facing = actor.position.x < m_state.player.position.x ? Facing::Right : Facing::Left;

        if (isBoss && actor.visible) {
            m_state.hud.showBossHealth = true;
            m_state.hud.bossHealth = actor.health;
        }

        if (actor.visible) {
            m_state.enemies.push_back(actor);
        }
    }

    m_state.hud.playerHealth = m_state.player.health;
    m_state.hud.playerEnergy = m_state.player.energy;
    m_state.hud.playerExhausted = m_exhaustedWarningTimer > 0.0f;
    m_state.result.defeatedEnemies = defeatedEnemies;
    m_state.progressRatio = kWorldWidth > 0.0f ? std::clamp(m_state.player.position.x / kWorldWidth, 0.0f, 1.0f) : 0.0f;
    m_state.map.cameraX = std::clamp(m_state.player.position.x - kViewportWidth * 0.42f,
                                     0.0f,
                                     std::max(0.0f, kWorldWidth - kViewportWidth));

    if (!player.alive) {
        m_state.phase = GamePhase::GameOver;
        m_state.result.gameOverReason = GameOverReason::PlayerDefeated;
        m_state.result.elapsedSeconds = m_state.elapsedSeconds;
        m_state.screenMessage.clear();
    } else if (m_sim->boss_defeated()) {
        m_state.phase = GamePhase::Win;
        m_state.result.winReason = WinReason::BossDefeated;
        m_state.result.elapsedSeconds = m_state.elapsedSeconds;
        m_state.screenMessage.clear();
    } else if (m_state.phase == GamePhase::Title) {
        m_state.screenMessage = "Press ENTER to Start";
    } else if (!m_paused) {
        m_state.phase = GamePhase::Playing;
        m_state.screenMessage.clear();
    }
}

std::function<void(bool)> GameViewModel::make_move_command(bool& flag)
{
    return [this, &flag](bool pressed) {
        flag = pressed;
        sync_state_from_simulation();
        notify_state_changed();
    };
}

void GameViewModel::begin_attack(ActorState state, float seconds, float energyCost)
{
    if (!m_sim || !is_gameplay_active()) {
        return;
    }

    if (!try_spend_energy(energyCost)) {
        sync_state_from_simulation();
        notify_state_changed();
        return;
    }

    m_attackState = state;
    m_attackTimer = seconds;
    m_sim->player_attack();
    sync_state_from_simulation();
    notify_state_changed();
}

void GameViewModel::reset_actions() noexcept
{
    m_jumpActive = false;
    m_jumpElapsed = 0.0f;
    m_attackTimer = 0.0f;
    m_playerEnergy = kMaxPlayerEnergy;
    m_exhaustedWarningTimer = 0.0f;
    m_attackState = ActorState::Idle;
    m_moveLeft = false;
    m_moveRight = false;
    m_moveUp = false;
    m_moveDown = false;
}

void GameViewModel::update_action_timers(float dt)
{
    m_playerEnergy = std::clamp(m_playerEnergy + kEnergyRegenPerSecond * dt, 0.0f, kMaxPlayerEnergy);
    m_exhaustedWarningTimer = std::max(0.0f, m_exhaustedWarningTimer - dt);

    if (m_jumpActive) {
        m_jumpElapsed += dt;
        if (m_jumpElapsed >= kJumpSeconds) {
            m_jumpActive = false;
            m_jumpElapsed = 0.0f;
        }
    }

    if (m_attackTimer > 0.0f) {
        m_attackTimer = std::max(0.0f, m_attackTimer - dt);
        if (m_attackTimer <= 0.0f) {
            m_attackState = ActorState::Idle;
        }
    }
}

bool GameViewModel::try_spend_energy(float cost) noexcept
{
    if (m_playerEnergy < cost) {
        m_exhaustedWarningTimer = kExhaustedWarningSeconds;
        return false;
    }

    m_playerEnergy = std::clamp(m_playerEnergy - cost, 0.0f, kMaxPlayerEnergy);
    m_exhaustedWarningTimer = 0.0f;
    return true;
}

bool GameViewModel::is_gameplay_active() const noexcept
{
    return !m_paused &&
           m_state.phase != GamePhase::Title &&
           m_state.phase != GamePhase::GameOver &&
           m_state.phase != GamePhase::Win;
}

void GameViewModel::notify_state_changed()
{
    fire(kGameSnapshotChangedEvent);
}

} // namespace alleyfist::viewmodel
