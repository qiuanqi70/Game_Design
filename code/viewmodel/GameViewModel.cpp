#include "GameViewModel.h"

#include "GameSimulation.h"

#include <algorithm>
#include <cmath>

namespace alleyfist::viewmodel {

namespace {

// 下面几个转换函数是 ViewModel 的“翻译层”：
// Simulation 保留规则语义，Common::GameState 保留显示语义。
ActorKind to_actor_kind(EntityKind kind)
{
    switch (kind) {
    case EntityKind::Player:
        return ActorKind::Player;
    case EntityKind::Patroller:
        return ActorKind::Patroller;
    case EntityKind::Ambusher:
        return ActorKind::Ambusher;
    case EntityKind::Charger:
        return ActorKind::Charger;
    case EntityKind::Ranged:
        return ActorKind::Ranged;
    case EntityKind::Boss:
        return ActorKind::Boss;
    }
    return ActorKind::Player;
}

ActorActionState to_enemy_action_state(const EntityState& entity)
{
    if (!entity.alive) {
        return ActorActionState::Dead;
    }

    switch (entity.behaviorState) {
    case EnemyBehaviorState::Hurt:
        return ActorActionState::Hurt;
    case EnemyBehaviorState::MeleeAttack:
        return ActorActionState::LightAttack;
    case EnemyBehaviorState::Ambush:
        return ActorActionState::Ambush;
    case EnemyBehaviorState::Charge:
        return ActorActionState::Charge;
    case EnemyBehaviorState::RangedAttack:
        return ActorActionState::RangedAttack;
    case EnemyBehaviorState::Idle:
        return ActorActionState::Idle;
    case EnemyBehaviorState::Patrol:
        return ActorActionState::Walk;
    }
    return ActorActionState::Idle;
}

ActorActionState to_player_action_state(PlayerBehaviorState state)
{
    switch (state) {
    case PlayerBehaviorState::Idle:
        return ActorActionState::Idle;
    case PlayerBehaviorState::Walk:
        return ActorActionState::Walk;
    case PlayerBehaviorState::Jump:
        return ActorActionState::Jump;
    case PlayerBehaviorState::LightAttack:
        return ActorActionState::LightAttack;
    case PlayerBehaviorState::HeavyAttack:
        return ActorActionState::HeavyAttack;
    case PlayerBehaviorState::AirAttack:
        return ActorActionState::AirAttack;
    case PlayerBehaviorState::Hurt:
        return ActorActionState::Hurt;
    case PlayerBehaviorState::Dead:
        return ActorActionState::Dead;
    }
    return ActorActionState::Idle;
}

} // namespace

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    sync_state_from_simulation();
}

GameViewModel::~GameViewModel() = default;

const GameState* GameViewModel::get_game_state() const noexcept
{
    return &m_state;
}

std::function<void(float, std::uint64_t)> GameViewModel::get_tick_command()
{
    // frameIndex 由 View 维护用于定时器节拍；当前规则只需要 delta time。
    return [this](float dt, std::uint64_t) {
        tick(dt);
    };
}

std::function<void(bool)> GameViewModel::get_move_left_command()
{
    return make_move_command(MoveInput::Left);
}

std::function<void(bool)> GameViewModel::get_move_right_command()
{
    return make_move_command(MoveInput::Right);
}

std::function<void(bool)> GameViewModel::get_move_up_command()
{
    return make_move_command(MoveInput::Up);
}

std::function<void(bool)> GameViewModel::get_move_down_command()
{
    return make_move_command(MoveInput::Down);
}

std::function<void()> GameViewModel::get_primary_action_command()
{
    return make_action_command(PlayerActionType::LightAttack);
}

std::function<void()> GameViewModel::get_secondary_action_command()
{
    return make_action_command(PlayerActionType::HeavyAttack);
}

std::function<void()> GameViewModel::get_state_toggle_command()
{
    return make_action_command(PlayerActionType::Jump);
}

std::function<void()> GameViewModel::get_reset_command()
{
    return [this]() {
        reset_game();
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_confirm_command()
{
    return [this]() {
        if (m_state.phase == GamePhase::Title ||
            m_state.phase == GamePhase::GameOver ||
            m_state.phase == GamePhase::Win) {
            reset_game();
        } else if (m_state.phase == GamePhase::Paused) {
            m_state.phase = GamePhase::Playing;
            m_state.screenMessage.clear();
        }
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::get_pause_command()
{
    return [this]() {
        if (m_state.phase != GamePhase::Playing &&
            m_state.phase != GamePhase::Paused) {
            return;
        }

        const bool pause = m_state.phase == GamePhase::Playing;
        if (pause) {
            m_sim->clear_movement_input();
        }
        m_state.phase = pause ? GamePhase::Paused : GamePhase::Playing;
        sync_state_from_simulation();
        m_state.screenMessage = pause ? "Paused" : "";
        notify_state_changed();
    };
}

void GameViewModel::tick(float dt)
{
    if (!is_gameplay_active()) {
        return;
    }

    m_sim->step(dt);
    sync_state_from_simulation();
    notify_state_changed();
}

void GameViewModel::sync_state_from_simulation()
{
    // 这里是 VM 的核心职责：把内部规则状态整理成稳定的 Common DTO。
    // View 只消费 m_state，因此敌人 AI、冷却、波次计时等内部字段不会泄漏出去。
    const float worldWidth = m_sim->world_width();
    const float streetTop = m_sim->street_top();
    const float streetBottom = m_sim->street_bottom();

    m_state.elapsedSeconds = m_sim->elapsed_seconds();
    m_state.map.streetTopY = streetTop;
    m_state.map.streetBottomY = streetBottom;

    const auto& entities = m_sim->entities();
    if (entities.empty()) {
        return;
    }

    const auto& player = entities.front();
    if (!player.alive || m_sim->boss_victory_ready()) {
        // 终局后立刻清空输入，避免玩家在 GameOver/Win 后继续积累移动状态。
        m_sim->clear_movement_input();
    }
    m_state.player.id = static_cast<std::uint32_t>(player.id);
    m_state.player.kind = ActorKind::Player;
    m_state.player.team = Team::Player;
    m_state.player.position = player.pos;
    m_state.player.position.x = std::clamp(
        m_state.player.position.x, 0.0f, worldWidth);
    m_state.player.position.laneY = std::clamp(
        m_state.player.position.laneY, streetTop, streetBottom);
    m_state.player.drawSize = {64.0f, 96.0f};
    m_state.player.health = {
        std::max(0, player.hp), player.maxHp > 0 ? player.maxHp : 100
    };
    m_state.player.visible = player.alive;
    m_state.player.actionState = to_player_action_state(
        m_sim->player_behavior_state());
    m_state.player.facing = player.facing;
    m_state.player.impactRevision = player.impactRevision;
    m_state.player.lastImpact = player.lastImpact;

    m_state.enemies.clear();
    m_state.projectiles.clear();
    m_state.pickups.clear();
    m_state.hud.showBossHealth = false;
    m_state.hud.bossHealth = {};
    m_state.enemies.reserve(entities.size() - 1u);

    std::uint32_t defeatedEnemies = 0;
    for (std::size_t i = 1; i < entities.size(); ++i) {
        const auto& entity = entities[i];
        if (!entity.alive) {
            ++defeatedEnemies;
        }

        const bool isBoss = entity.kind == EntityKind::Boss;
        const int fallbackMaxHealth = isBoss ? 140 : 20;

        ActorState actor;
        actor.id = static_cast<std::uint32_t>(entity.id);
        actor.kind = to_actor_kind(entity.kind);
        actor.visualVariant = entity.visualVariant;
        actor.team = Team::Enemy;
        actor.position = entity.pos;
        actor.position.laneY = std::clamp(
            actor.position.laneY, streetTop, streetBottom);
        actor.drawSize = isBoss ? Size{88.0f, 128.0f}
                                : Size{48.0f, 72.0f};
        actor.health = {
            std::max(0, entity.hp),
            entity.maxHp > 0 ? entity.maxHp : fallbackMaxHealth
        };
        actor.actionState = to_enemy_action_state(entity);
        actor.visible = entity.alive || entity.deathTimer > 0.0f;
        actor.facing = entity.facing;
        actor.impactRevision = entity.impactRevision;
        actor.lastImpact = entity.lastImpact;

        if (isBoss && entity.alive) {
            // Boss 血条是 HUD 的特殊显示，不需要 View 再去遍历敌人判断。
            m_state.hud.showBossHealth = true;
            m_state.hud.bossHealth = actor.health;
        }
        if (actor.visible) {
            m_state.enemies.push_back(actor);
        }
    }

    const auto& projectiles = m_sim->projectiles();
    m_state.projectiles.reserve(projectiles.size());
    for (const auto& projectile : projectiles) {
        ProjectileState state;
        state.id = projectile.id;
        state.kind = ProjectileKind::ThrownObject;
        state.visualVariant = projectile.visualVariant;
        state.team = Team::Enemy;
        state.position = projectile.position;
        state.facing = projectile.facing;
        m_state.projectiles.push_back(state);
    }

    const auto& pickups = m_sim->pickups();
    m_state.pickups.reserve(pickups.size());
    for (const auto& pickup : pickups) {
        PickupState state;
        state.id = pickup.id;
        state.kind = pickup.kind;
        state.position = pickup.position;
        // 拾取物的上下浮动是纯表现效果，放在 VM 快照里供 View 直接绘制。
        state.position.z += std::sin(
            m_state.elapsedSeconds * 2.2f +
            static_cast<float>(pickup.id) * 0.6f) * 8.0f;
        m_state.pickups.push_back(state);
    }

    m_state.encounter = m_sim->encounter_state();
    m_state.hud.playerHealth = m_state.player.health;
    m_state.hud.playerEnergy = {
        static_cast<int>(std::lround(m_sim->player_energy())),
        static_cast<int>(std::lround(m_sim->player_max_energy()))
    };
    m_state.hud.playerExhausted = m_sim->player_exhausted();
    m_state.result.defeatedEnemies = defeatedEnemies;
    m_state.progressRatio = worldWidth > 0.0f
                                ? std::clamp(
                                      m_state.player.position.x / worldWidth,
                                      0.0f, 1.0f)
                                : 0.0f;
    // 镜头跟随玩家但保持在地图范围内，View 只做 world->screen 转换。
    m_state.map.cameraX = std::clamp(
        m_state.player.position.x - m_state.map.viewportWidth * 0.42f,
        0.0f,
        std::max(0.0f, worldWidth - m_state.map.viewportWidth));

    if (!player.alive) {
        m_state.phase = GamePhase::GameOver;
        m_state.result.elapsedSeconds = m_state.elapsedSeconds;
        m_state.screenMessage.clear();
    } else if (m_sim->boss_victory_ready()) {
        m_state.phase = GamePhase::Win;
        m_state.result.elapsedSeconds = m_state.elapsedSeconds;
        m_state.screenMessage.clear();
    } else if (m_state.phase == GamePhase::Title) {
        m_state.screenMessage = "Press ENTER to Start";
    } else if (m_state.phase != GamePhase::Paused) {
        m_state.phase = GamePhase::Playing;
        m_state.screenMessage.clear();
    }
}

std::function<void(bool)> GameViewModel::make_move_command(MoveInput input)
{
    return [this, input](bool pressed) {
        if (pressed && !is_gameplay_active()) {
            return;
        }
        // 方向输入是持续状态，按下和释放都必须同步给模拟器。
        switch (input) {
        case MoveInput::Left:
            m_sim->set_move_left(pressed);
            break;
        case MoveInput::Right:
            m_sim->set_move_right(pressed);
            break;
        case MoveInput::Up:
            m_sim->set_move_up(pressed);
            break;
        case MoveInput::Down:
            m_sim->set_move_down(pressed);
            break;
        }
        sync_state_from_simulation();
        notify_state_changed();
    };
}

std::function<void()> GameViewModel::make_action_command(PlayerActionType action)
{
    return [this, action]() {
        if (!is_gameplay_active()) {
            return;
        }
        // 动作请求可能因为能量不足、受击或攻击中被拒绝；模拟器负责判定。
        m_sim->request_player_action(action);
        sync_state_from_simulation();
        notify_state_changed();
    };
}

void GameViewModel::reset_game()
{
    m_sim->reset();
    m_state = {};
    m_state.phase = GamePhase::Playing;
    sync_state_from_simulation();
}

bool GameViewModel::is_gameplay_active() const noexcept
{
    return m_state.phase == GamePhase::Playing;
}

void GameViewModel::notify_state_changed()
{
    fire(kGameStateChangedEvent);
}

} // namespace alleyfist::viewmodel
