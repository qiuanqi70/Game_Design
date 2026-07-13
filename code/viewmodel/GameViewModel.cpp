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

} // namespace

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    m_sim->start();
    sync_state_from_simulation();
}

GameViewModel::~GameViewModel() = default;

const GameViewState* GameViewModel::get_game_state() const noexcept
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

std::function<void()> GameViewModel::get_primary_action_command()
{
    return [this]() {
        if (m_sim && !m_paused) {
            m_sim->player_attack();
            sync_state_from_simulation();
            notify_state_changed();
        }
    };
}

std::function<void()> GameViewModel::get_secondary_action_command()
{
    return get_primary_action_command();
}

std::function<void()> GameViewModel::get_state_toggle_command()
{
    return [this]() {
        m_state.player.state = ActorState::Jump;
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_reset_command()
{
    return [this]() {
        if (m_sim) {
            m_sim->reset();
            m_sim->start();
        }
        m_paused = false;
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
    if (!m_sim || m_paused) {
        return;
    }

    if (m_state.phase == GamePhase::Title) {
        return;
    }

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
    m_state.player.energy = {100, 100};
    m_state.player.visible = player.alive;
    m_state.player.state = player.alive ? ((m_moveLeft || m_moveRight || m_moveUp || m_moveDown) ? ActorState::Walk : ActorState::Idle)
                                        : ActorState::Dead;
    if (m_moveLeft && !m_moveRight) {
        m_state.player.facing = Facing::Left;
    } else if (m_moveRight && !m_moveLeft) {
        m_state.player.facing = Facing::Right;
    }

    m_state.enemies.clear();
    for (std::size_t i = 1; i < list.size(); ++i) {
        const auto& entity = list[i];
        ActorViewState actor;
        actor.kind = ActorKind::Grunt;
        actor.team = Team::Enemy;
        actor.position = entity.pos;
        actor.position.laneY = std::clamp(actor.position.laneY, kStreetTop, kStreetBottom);
        actor.drawSize = {48.0f, 72.0f};
        actor.health = {std::max(0, entity.hp), entity.maxHp > 0 ? entity.maxHp : 20};
        actor.state = entity.alive ? ActorState::Walk : ActorState::Dead;
        actor.visible = entity.alive;
        actor.facing = actor.position.x < m_state.player.position.x ? Facing::Right : Facing::Left;
        if (actor.visible) {
            m_state.enemies.push_back(actor);
        }
    }

    m_state.hud.playerHealth = m_state.player.health;
    m_state.hud.playerEnergy = m_state.player.energy;
    m_state.hud.showBossHealth = false;
    m_state.progressRatio = kWorldWidth > 0.0f ? std::clamp(m_state.player.position.x / kWorldWidth, 0.0f, 1.0f) : 0.0f;
    m_state.map.cameraX = std::clamp(m_state.player.position.x - kViewportWidth * 0.42f,
                                     0.0f,
                                     std::max(0.0f, kWorldWidth - kViewportWidth));

    if (!player.alive) {
        m_state.phase = GamePhase::GameOver;
        m_state.result.gameOverReason = GameOverReason::PlayerDefeated;
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

void GameViewModel::notify_state_changed()
{
    fire(kGameViewStateChangedEvent);
}

} // namespace alleyfist::viewmodel
