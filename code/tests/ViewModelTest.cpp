#include "GameSimulation.h"
#include "GameViewModel.h"

#include <QCoreApplication>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace alleyfist::viewmodel {
namespace {

// 模拟辅助函数统一使用 20 ms 固定步长，减少浮点和帧率差异造成的波动。
constexpr float kStep = 0.02f;

// 浮点状态不能直接判等，此函数用于位置、能量和时间断言。
bool nearly_equal(float first, float second, float tolerance = 0.001f)
{
    return std::abs(first - second) <= tolerance;
}

// 按稳定 ID 查找实体，供跨多帧测试重新取得可能已搬迁的 vector 元素。
const EntityState* entity_by_id(const GameSimulation& simulation, int id)
{
    const auto& entities = simulation.entities();
    const auto found = std::find_if(
        entities.begin(), entities.end(), [id](const EntityState& entity) {
            return entity.id == id;
        });
    return found == entities.end() ? nullptr : &*found;
}

// 查找指定类型的第一个存活实体，用于选取当前波次的测试目标。
const EntityState* first_alive_entity(const GameSimulation& simulation,
                                      EntityKind kind)
{
    const auto& entities = simulation.entities();
    const auto found = std::find_if(
        entities.begin(), entities.end(), [kind](const EntityState& entity) {
            return entity.alive && entity.kind == kind;
        });
    return found == entities.end() ? nullptr : &*found;
}

// 统计当前存活的普通敌人，不把玩家和 Boss 计入波次清理条件。
std::uint32_t alive_regular_enemy_count(const GameSimulation& simulation)
{
    return static_cast<std::uint32_t>(std::count_if(
        simulation.entities().begin(), simulation.entities().end(),
        [](const EntityState& entity) {
            return entity.alive && entity.kind != EntityKind::Player &&
                   entity.kind != EntityKind::Boss;
        }));
}

// 推进模拟直到条件成立或达到步数上限，避免测试无限等待状态转换。
template <typename Predicate>
bool step_until(GameSimulation& simulation, Predicate predicate,
                int maximumSteps, float dt = kStep)
{
    if (predicate()) {
        return true;
    }
    for (int step = 0; step < maximumSteps; ++step) {
        simulation.step(dt);
        if (predicate()) {
            return true;
        }
    }
    return false;
}

// 按固定小步长推进指定时长，用于验证计时器、恢复和动作结束行为。
void step_for(GameSimulation& simulation, float seconds, float dt = kStep)
{
    float remaining = seconds;
    while (remaining > 0.0f) {
        const float currentStep = std::min(dt, remaining);
        simulation.step(currentStep);
        remaining -= currentStep;
    }
}

// 根据目标偏移设置四方向输入；进入容差范围后不再继续移动。
void set_movement_towards(GameSimulation& simulation, float dx, float dy,
                          float toleranceX = 2.0f,
                          float toleranceY = 2.0f)
{
    simulation.set_move_left(dx < -toleranceX);
    simulation.set_move_right(dx > toleranceX);
    simulation.set_move_up(dy < -toleranceY);
    simulation.set_move_down(dy > toleranceY);
}

// 驱动玩家到指定世界坐标，供拾取物和关卡位置相关测试复用。
bool move_player_to(GameSimulation& simulation, float targetX, float targetY,
                    float toleranceX = 3.0f, float toleranceY = 3.0f,
                    int maximumSteps = 2000)
{
    for (int step = 0; step < maximumSteps; ++step) {
        const auto& player = simulation.entities().front();
        const float dx = targetX - player.pos.x;
        const float dy = targetY - player.pos.laneY;
        if (std::abs(dx) <= toleranceX && std::abs(dy) <= toleranceY) {
            simulation.clear_movement_input();
            return true;
        }
        if (!player.alive) {
            simulation.clear_movement_input();
            return false;
        }
        set_movement_towards(simulation, dx, dy, toleranceX, toleranceY);
        simulation.step(kStep);
    }
    simulation.clear_movement_input();
    return false;
}

// 驱动玩家进入目标实体的交互范围，供近战、受击和 AI 测试复用。
bool move_near_entity(GameSimulation& simulation, int entityId,
                      float rangeX = 24.0f, float rangeY = 12.0f,
                      int maximumSteps = 2500)
{
    for (int step = 0; step < maximumSteps; ++step) {
        const EntityState* target = entity_by_id(simulation, entityId);
        if (target == nullptr || !target->alive) {
            simulation.clear_movement_input();
            return false;
        }
        const auto& player = simulation.entities().front();
        const float dx = target->pos.x - player.pos.x;
        const float dy = target->pos.laneY - player.pos.laneY;
        if (std::abs(dx) <= rangeX && std::abs(dy) <= rangeY) {
            simulation.clear_movement_input();
            return true;
        }
        if (!player.alive) {
            simulation.clear_movement_input();
            return false;
        }
        set_movement_towards(simulation, dx, dy, rangeX, rangeY);
        simulation.step(kStep);
    }
    simulation.clear_movement_input();
    return false;
}

// 把玩家放到可命中敌人但不会立即拾取掉落物的位置。
bool move_to_safe_attack_edge(GameSimulation& simulation, int entityId,
                              int maximumSteps = 2500)
{
    for (int step = 0; step < maximumSteps; ++step) {
        const EntityState* target = entity_by_id(simulation, entityId);
        if (target == nullptr || !target->alive) {
            simulation.clear_movement_input();
            return false;
        }
        const float targetX = target->pos.x - 42.0f;
        const float targetY = std::clamp(
            target->pos.laneY + 30.0f,
            simulation.street_top(), simulation.street_bottom());
        const auto& player = simulation.entities().front();
        const float dx = targetX - player.pos.x;
        const float dy = targetY - player.pos.laneY;
        if (std::abs(dx) <= 2.0f && std::abs(dy) <= 2.0f) {
            simulation.clear_movement_input();
            return true;
        }
        if (!player.alive) {
            simulation.clear_movement_input();
            return false;
        }
        set_movement_towards(simulation, dx, dy);
        simulation.step(kStep);
    }
    simulation.clear_movement_input();
    return false;
}

// 只通过正常移动和轻攻击击败实体，不直接修改生命值。
bool defeat_entity(GameSimulation& simulation, int entityId,
                   int maximumSteps = 5000,
                   bool keepPickupOutOfReach = false)
{
    for (int step = 0; step < maximumSteps; ++step) {
        const EntityState* target = entity_by_id(simulation, entityId);
        if (target == nullptr || !target->alive) {
            simulation.clear_movement_input();
            return true;
        }
        if (!simulation.entities().front().alive) {
            simulation.clear_movement_input();
            return false;
        }

        const bool positioned = keepPickupOutOfReach
                                    ? move_to_safe_attack_edge(
                                          simulation, entityId, 1)
                                    : move_near_entity(
                                          simulation, entityId, 30.0f, 16.0f, 1);
        if (positioned) {
            simulation.request_player_action(PlayerActionType::LightAttack);
        }
        simulation.step(kStep);
    }
    simulation.clear_movement_input();
    return false;
}

// 按远程、冲锋、伏击、巡逻的顺序选目标，降低清理波次时受到的伤害。
const EntityState* preferred_regular_target(const GameSimulation& simulation)
{
    constexpr EntityKind priority[] = {
        EntityKind::Ranged,
        EntityKind::Charger,
        EntityKind::Ambusher,
        EntityKind::Patroller
    };
    for (const EntityKind kind : priority) {
        if (const EntityState* target = first_alive_entity(simulation, kind)) {
            return target;
        }
    }
    return nullptr;
}

// 击败当前波次全部普通敌人，供波次和 Boss 流程测试搭建前置状态。
bool defeat_current_regular_wave(GameSimulation& simulation)
{
    for (int enemy = 0; enemy < 8; ++enemy) {
        const EntityState* target = preferred_regular_target(simulation);
        if (target == nullptr) {
            return true;
        }
        const int id = target->id;
        if (!defeat_entity(simulation, id)) {
            return false;
        }
    }
    return alive_regular_enemy_count(simulation) == 0;
}

// 等待指定波次真正生成敌人，而不只检查波次编号是否改变。
bool wait_for_wave(GameSimulation& simulation, std::uint32_t wave)
{
    return step_until(
        simulation,
        [&simulation, wave]() {
            return simulation.encounter_state().currentWave == wave &&
                   alive_regular_enemy_count(simulation) > 0;
        },
        150);
}

// 通过合法战斗依次清理前置波次，进入目标普通波次。
bool reach_wave(GameSimulation& simulation, std::uint32_t wave)
{
    for (std::uint32_t currentWave = 1; currentWave < wave; ++currentWave) {
        if (!defeat_current_regular_wave(simulation) ||
            !wait_for_wave(simulation, currentWave + 1)) {
            return false;
        }
    }
    return simulation.encounter_state().currentWave == wave &&
           alive_regular_enemy_count(simulation) > 0;
}

// 清理第二波其他敌人并调整站位，构造只受一个远程敌人影响的场景。
bool prepare_isolated_ranged_enemy(GameSimulation& simulation, int& rangedId)
{
    if (!reach_wave(simulation, 2)) {
        return false;
    }

    std::vector<int> otherEnemyIds;
    for (const auto& entity : simulation.entities()) {
        if (entity.alive && entity.kind != EntityKind::Player &&
            entity.kind != EntityKind::Ranged) {
            otherEnemyIds.push_back(entity.id);
        }
    }
    for (const int id : otherEnemyIds) {
        if (!defeat_entity(simulation, id)) {
            return false;
        }
    }

    const EntityState* ranged = first_alive_entity(simulation, EntityKind::Ranged);
    if (ranged == nullptr) {
        return false;
    }
    rangedId = ranged->id;

    for (int step = 0; step < 2500; ++step) {
        ranged = entity_by_id(simulation, rangedId);
        if (ranged == nullptr || !ranged->alive) {
            return false;
        }
        const float targetX = ranged->pos.x - 140.0f;
        const float targetY = ranged->pos.laneY;
        const auto& player = simulation.entities().front();
        const float dx = targetX - player.pos.x;
        const float dy = targetY - player.pos.laneY;
        if (std::abs(dx) <= 3.0f && std::abs(dy) <= 3.0f) {
            simulation.clear_movement_input();
            break;
        }
        set_movement_towards(simulation, dx, dy);
        simulation.step(kStep);
    }
    simulation.clear_movement_input();

    return step_until(
        simulation,
        [&simulation, rangedId]() {
            const EntityState* current = entity_by_id(simulation, rangedId);
            return current != nullptr && current->alive &&
                   current->behaviorState == EnemyBehaviorState::Idle &&
                   simulation.projectiles().empty();
        },
        300);
}

// 记录当前最大投射物 ID，用于区分随后新发射的投射物。
std::uint32_t maximum_projectile_id(const GameSimulation& simulation)
{
    std::uint32_t maximum = 0;
    for (const auto& projectile : simulation.projectiles()) {
        maximum = std::max(maximum, projectile.id);
    }
    return maximum;
}

// 查找基准 ID 之后生成的投射物。
const ProjectileVm* projectile_after(const GameSimulation& simulation,
                                     std::uint32_t previousMaximum)
{
    const auto& projectiles = simulation.projectiles();
    const auto found = std::find_if(
        projectiles.begin(), projectiles.end(),
        [previousMaximum](const ProjectileVm& projectile) {
            return projectile.id > previousMaximum;
        });
    return found == projectiles.end() ? nullptr : &*found;
}

// 推进模拟直到远程敌人生成新投射物，并返回其稳定 ID。
bool wait_for_new_projectile(GameSimulation& simulation,
                             std::uint32_t previousMaximum,
                             std::uint32_t& projectileId)
{
    const bool spawned = step_until(
        simulation,
        [&simulation, previousMaximum]() {
            return projectile_after(simulation, previousMaximum) != nullptr;
        },
        250);
    if (!spawned) {
        return false;
    }
    projectileId = projectile_after(simulation, previousMaximum)->id;
    return true;
}

// 判断指定投射物是否仍在模拟中，用于观察命中或过期移除。
bool projectile_exists(const GameSimulation& simulation, std::uint32_t id)
{
    return std::any_of(
        simulation.projectiles().begin(), simulation.projectiles().end(),
        [id](const ProjectileVm& projectile) { return projectile.id == id; });
}

} // namespace

class ViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    // 测试 Simulation 的初始实体/资源、非法 dt 防御，以及 reset 的完整恢复。
    void simulationInitialStateResetAndInvalidDelta()
    {
        GameSimulation simulation;
        QCOMPARE(simulation.entities().size(), std::size_t{5});
        QCOMPARE(simulation.entities().front().id, 1);
        QVERIFY(simulation.entities().front().kind == EntityKind::Player);
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 80.0f));
        QVERIFY(nearly_equal(simulation.entities().front().pos.laneY, 400.0f));
        QVERIFY(nearly_equal(simulation.player_energy(), 100.0f));
        QVERIFY(nearly_equal(simulation.elapsed_seconds(), 0.0f));

        const auto initialPosition = simulation.entities().front().pos;
        simulation.step(0.0f);
        simulation.step(-0.1f);
        simulation.step(std::numeric_limits<float>::quiet_NaN());
        simulation.step(std::numeric_limits<float>::infinity());
        QVERIFY(nearly_equal(simulation.elapsed_seconds(), 0.0f));
        QVERIFY(nearly_equal(simulation.entities().front().pos.x,
                             initialPosition.x));

        simulation.set_move_right(true);
        QVERIFY(simulation.request_player_action(PlayerActionType::Jump));
        simulation.step(0.1f);
        QVERIFY(simulation.entities().front().pos.x > initialPosition.x);
        QVERIFY(simulation.entities().front().pos.z > 0.0f);
        QVERIFY(simulation.player_energy() < simulation.player_max_energy());

        simulation.reset();
        QCOMPARE(simulation.entities().size(), std::size_t{5});
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 80.0f));
        QVERIFY(nearly_equal(simulation.entities().front().pos.laneY, 400.0f));
        QVERIFY(nearly_equal(simulation.entities().front().pos.z, 0.0f));
        QVERIFY(nearly_equal(simulation.player_energy(), 100.0f));
        QVERIFY(nearly_equal(simulation.elapsed_seconds(), 0.0f));
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Idle);
    }

    // 测试 ViewModel 生命周期命令、阶段切换、状态同步和通知订阅/移除。
    void viewModelLifecycleCommandsAndNotifications()
    {
        GameViewModel viewModel;
        const GameState* state = viewModel.get_game_state();
        QVERIFY(state != nullptr);
        QVERIFY(state->phase == GamePhase::Title);
        QVERIFY(state->screenMessage == "Press ENTER to Start");
        QCOMPARE(state->enemies.size(), std::size_t{4});
        QCOMPARE(state->hud.playerHealth.current, 100);

        int notifications = 0;
        const std::uintptr_t cookie = viewModel.add_notification(
            [&notifications](std::uint32_t eventId) {
                if (eventId == kGameStateChangedEvent) {
                    ++notifications;
                }
            });

        viewModel.get_move_right_command()(true);
        QCOMPARE(notifications, 0);
        viewModel.get_move_right_command()(false);
        QCOMPARE(notifications, 1);

        viewModel.get_confirm_command()();
        QVERIFY(state->phase == GamePhase::Playing);
        QCOMPARE(notifications, 2);
        const float initialElapsed = state->elapsedSeconds;

        viewModel.get_tick_command()(-1.0f, 7u);
        QVERIFY(nearly_equal(state->elapsedSeconds, initialElapsed));
        QCOMPARE(notifications, 3);

        viewModel.get_pause_command()();
        QVERIFY(state->phase == GamePhase::Paused);
        QVERIFY(state->screenMessage == "Paused");
        const float pausedElapsed = state->elapsedSeconds;
        viewModel.get_move_right_command()(true);
        viewModel.get_tick_command()(0.1f, 8u);
        QVERIFY(nearly_equal(state->elapsedSeconds, pausedElapsed));

        viewModel.get_confirm_command()();
        QVERIFY(state->phase == GamePhase::Playing);
        viewModel.get_tick_command()(0.1f, 9u);
        QVERIFY(state->elapsedSeconds > pausedElapsed);
        QVERIFY(nearly_equal(state->player.position.x, 80.0f));

        viewModel.get_move_right_command()(true);
        viewModel.get_tick_command()(0.1f, 10u);
        viewModel.get_move_right_command()(false);
        QVERIFY(state->player.position.x > 80.0f);

        viewModel.get_reset_command()();
        QVERIFY(state->phase == GamePhase::Playing);
        QVERIFY(nearly_equal(state->elapsedSeconds, 0.0f));
        QVERIFY(nearly_equal(state->player.position.x, 80.0f));
        QCOMPARE(state->hud.playerEnergy.current, 100);
        viewModel.remove_notification(cookie);
    }

    // 测试 ViewModel 将轻击、重击、跳跃和空中攻击映射到公共状态与 HUD。
    void viewModelMapsActionCommandsAndHudState()
    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();
        const GameState* state = viewModel.get_game_state();

        viewModel.get_primary_action_command()();
        QVERIFY(state->player.actionState == ActorActionState::LightAttack);
        QCOMPARE(state->hud.playerEnergy.current, 88);
        viewModel.get_secondary_action_command()();
        QVERIFY(state->player.actionState == ActorActionState::LightAttack);
        QCOMPARE(state->hud.playerEnergy.current, 88);
        viewModel.get_tick_command()(0.2f, 1u);
        QVERIFY(state->player.actionState == ActorActionState::Idle);

        viewModel.get_reset_command()();
        viewModel.get_secondary_action_command()();
        QVERIFY(state->player.actionState == ActorActionState::HeavyAttack);
        QCOMPARE(state->hud.playerEnergy.current, 75);

        viewModel.get_reset_command()();
        viewModel.get_state_toggle_command()();
        QVERIFY(state->player.actionState == ActorActionState::Jump);
        QCOMPARE(state->hud.playerEnergy.current, 82);
        viewModel.get_tick_command()(0.1f, 2u);
        QVERIFY(state->player.position.z > 40.0f);
        viewModel.get_primary_action_command()();
        QVERIFY(state->player.actionState == ActorActionState::AirAttack);
        QVERIFY(state->hud.playerEnergy.current < 82);
    }

    // 测试四方向位移、相反输入抵消，以及未清波时地图门禁限制。
    void movementDirectionsOppositesAndWaveGate()
    {
        GameSimulation simulation;
        simulation.set_move_right(true);
        simulation.step(0.1f);
        simulation.set_move_right(false);
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 102.0f));

        simulation.set_move_left(true);
        simulation.step(0.1f);
        simulation.set_move_left(false);
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 80.0f));

        simulation.set_move_up(true);
        simulation.step(0.1f);
        simulation.set_move_up(false);
        QVERIFY(nearly_equal(simulation.entities().front().pos.laneY, 378.0f));

        simulation.set_move_down(true);
        simulation.step(0.1f);
        simulation.set_move_down(false);
        QVERIFY(nearly_equal(simulation.entities().front().pos.laneY, 400.0f));

        simulation.set_move_left(true);
        simulation.set_move_right(true);
        simulation.set_move_up(true);
        simulation.set_move_down(true);
        simulation.step(0.1f);
        simulation.clear_movement_input();
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 80.0f));
        QVERIFY(nearly_equal(simulation.entities().front().pos.laneY, 400.0f));
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Idle);

        simulation.set_move_right(true);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return nearly_equal(simulation.entities().front().pos.x, 760.0f);
            },
            400));
        simulation.clear_movement_input();
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 760.0f));
        simulation.step(0.5f);
        QVERIFY(nearly_equal(simulation.entities().front().pos.x, 760.0f));
    }

    // 测试轻重攻击各自的耗能、命中帧、伤害、冲击等级和单次命中约束。
    void lightAndHeavyAttacksUseDistinctHitFrames()
    {
        GameSimulation lightSimulation;
        const int lightTargetId = lightSimulation.entities().at(1).id;
        QVERIFY2(move_to_safe_attack_edge(lightSimulation, lightTargetId),
                 "could not reach the light-attack test target");
        const int lightInitialHealth = entity_by_id(
            lightSimulation, lightTargetId)->hp;
        QVERIFY(lightSimulation.request_player_action(
            PlayerActionType::LightAttack));
        QVERIFY(lightSimulation.player_behavior_state() ==
                PlayerBehaviorState::LightAttack);
        QVERIFY(nearly_equal(lightSimulation.player_energy(), 88.0f));
        const float lockedEnergy = lightSimulation.player_energy();
        QVERIFY(!lightSimulation.request_player_action(
            PlayerActionType::HeavyAttack));
        QVERIFY(nearly_equal(lightSimulation.player_energy(), lockedEnergy));

        lightSimulation.step(0.079f);
        QCOMPARE(entity_by_id(lightSimulation, lightTargetId)->hp,
                 lightInitialHealth);
        lightSimulation.step(0.002f);
        QCOMPARE(entity_by_id(lightSimulation, lightTargetId)->hp,
                 lightInitialHealth - 10);
        QVERIFY(entity_by_id(lightSimulation, lightTargetId)->lastImpact ==
                ImpactLevel::Light);
        const std::uint32_t lightRevision = entity_by_id(
            lightSimulation, lightTargetId)->impactRevision;
        lightSimulation.step(0.2f);
        QCOMPARE(entity_by_id(lightSimulation, lightTargetId)->hp,
                 lightInitialHealth - 10);
        QCOMPARE(entity_by_id(lightSimulation, lightTargetId)->impactRevision,
                 lightRevision);

        GameSimulation heavySimulation;
        const int heavyTargetId = heavySimulation.entities().at(1).id;
        QVERIFY2(move_to_safe_attack_edge(heavySimulation, heavyTargetId),
                 "could not reach the heavy-attack test target");
        const int heavyInitialHealth = entity_by_id(
            heavySimulation, heavyTargetId)->hp;
        QVERIFY(heavySimulation.request_player_action(
            PlayerActionType::HeavyAttack));
        QVERIFY(heavySimulation.player_behavior_state() ==
                PlayerBehaviorState::HeavyAttack);
        QVERIFY(nearly_equal(heavySimulation.player_energy(), 75.0f));
        heavySimulation.step(0.179f);
        QCOMPARE(entity_by_id(heavySimulation, heavyTargetId)->hp,
                 heavyInitialHealth);
        heavySimulation.step(0.002f);
        QCOMPARE(entity_by_id(heavySimulation, heavyTargetId)->hp,
                 heavyInitialHealth - 18);
        QVERIFY(entity_by_id(heavySimulation, heavyTargetId)->lastImpact ==
                ImpactLevel::Heavy);
        const std::uint32_t heavyRevision = entity_by_id(
            heavySimulation, heavyTargetId)->impactRevision;
        heavySimulation.step(0.2f);
        QCOMPARE(entity_by_id(heavySimulation, heavyTargetId)->impactRevision,
                 heavyRevision);
    }

    // 测试玩家受击期间动作被中断和拒绝，并在硬直结束后重新接受动作。
    void playerHurtStateRejectsAndInterruptsActions()
    {
        GameSimulation simulation;
        const int patrollerId = simulation.entities().at(1).id;
        QVERIFY2(move_near_entity(simulation, patrollerId, 8.0f, 8.0f),
                 "could not enter patroller contact range");
        QVERIFY2(step_until(
            simulation,
            [&simulation]() {
                return simulation.entities().front().impactRevision > 0;
            },
            100),
            "patroller did not hit the player");

        const auto& hurtPlayer = simulation.entities().front();
        QVERIFY(hurtPlayer.hurtTimer > 0.0f);
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Hurt);
        const float energy = simulation.player_energy();
        QVERIFY(!simulation.request_player_action(
            PlayerActionType::LightAttack));
        QVERIFY(!simulation.request_player_action(PlayerActionType::Jump));
        QVERIFY(nearly_equal(simulation.player_energy(), energy));

        const std::uint32_t revision = hurtPlayer.impactRevision;
        simulation.step(0.1f);
        QCOMPARE(simulation.entities().front().impactRevision, revision);
        QVERIFY2(step_until(
            simulation,
            [&simulation]() {
                return simulation.entities().front().hurtTimer <= 0.0f;
            },
            20),
            "player hurt lock did not expire");
        QVERIFY(simulation.request_player_action(PlayerActionType::Jump));
    }

    // 测试连续重击触发精疲力竭、低能量时拒绝动作，以及恢复阈值后的解锁。
    void energyExhaustionAndRecovery()
    {
        GameSimulation simulation;
        int acceptedActions = 0;
        while (acceptedActions < 10 && simulation.request_player_action(
                   PlayerActionType::HeavyAttack)) {
            ++acceptedActions;
            step_for(simulation, 0.31f);
        }
        QVERIFY(acceptedActions >= 4);
        QVERIFY(simulation.player_energy() < 25.0f);
        QVERIFY(simulation.player_exhausted());
        QVERIFY(!simulation.request_player_action(
            PlayerActionType::HeavyAttack));

        step_for(simulation, 1.0f);
        QVERIFY(!simulation.player_exhausted());
        QVERIFY(simulation.player_energy() >= 25.0f);
        QVERIFY(simulation.request_player_action(
            PlayerActionType::HeavyAttack));
    }

    // 测试跳跃独占高度变化、可转为空中攻击，并最终落地回到 Idle。
    void jumpOwnsHeightAndSupportsAirAttack()
    {
        GameSimulation simulation;
        QVERIFY(simulation.request_player_action(PlayerActionType::Jump));
        QVERIFY(!simulation.request_player_action(PlayerActionType::Jump));
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Jump);
        QVERIFY(nearly_equal(simulation.player_energy(), 82.0f));

        QVERIFY(simulation.request_player_action(
            PlayerActionType::LightAttack));
        QVERIFY(simulation.player_behavior_state() ==
                PlayerBehaviorState::AirAttack);
        QVERIFY(nearly_equal(simulation.player_energy(), 70.0f));
        QVERIFY(!simulation.request_player_action(
            PlayerActionType::HeavyAttack));

        simulation.step(0.1f);
        QVERIFY(simulation.entities().front().pos.z > 40.0f);
        QVERIFY(simulation.player_behavior_state() ==
                PlayerBehaviorState::AirAttack);
        simulation.step(0.1f);
        QVERIFY(simulation.entities().front().pos.z > 75.0f);
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Jump);
        simulation.step(0.075f);
        QVERIFY(simulation.entities().front().pos.z > 88.0f);
        step_for(simulation, 0.3f);
        QVERIFY(nearly_equal(simulation.entities().front().pos.z, 0.0f));
        QVERIFY(simulation.player_behavior_state() == PlayerBehaviorState::Idle);
    }

    // 测试巡逻近战、伏击、冲锋、远程攻击等 AI 状态均可到达并能发射投射物。
    void enemyBehaviorStatesAndRangedProjectileAreReachable()
    {
        GameSimulation simulation;
        const int patrollerId = simulation.entities().at(1).id;
        QVERIFY(move_near_entity(simulation, patrollerId, 20.0f, 15.0f));
        QVERIFY(step_until(
            simulation,
            [&simulation, patrollerId]() {
                const EntityState* patroller = entity_by_id(
                    simulation, patrollerId);
                return patroller != nullptr &&
                       patroller->behaviorState ==
                           EnemyBehaviorState::MeleeAttack;
            },
            30));

        GameSimulation waveSimulation;
        QVERIFY2(reach_wave(waveSimulation, 2),
                 "could not reach the second enemy wave");
        const EntityState* ranged = first_alive_entity(
            waveSimulation, EntityKind::Ranged);
        QVERIFY(ranged != nullptr);
        QVERIFY(ranged->visualVariant == ActorVisualVariant::RangedGunner);

        const EntityState* ambusher = first_alive_entity(
            waveSimulation, EntityKind::Ambusher);
        QVERIFY(ambusher != nullptr);
        const int ambusherId = ambusher->id;
        QVERIFY(move_near_entity(waveSimulation, ambusherId, 100.0f, 60.0f));
        QVERIFY(step_until(
            waveSimulation,
            [&waveSimulation, ambusherId]() {
                const EntityState* current = entity_by_id(
                    waveSimulation, ambusherId);
                return current != nullptr &&
                       current->behaviorState == EnemyBehaviorState::Ambush;
            },
            50));

        const EntityState* charger = first_alive_entity(
            waveSimulation, EntityKind::Charger);
        QVERIFY(charger != nullptr);
        const int chargerId = charger->id;
        QVERIFY(move_near_entity(waveSimulation, chargerId, 100.0f, 50.0f));
        QVERIFY(step_until(
            waveSimulation,
            [&waveSimulation, chargerId]() {
                const EntityState* current = entity_by_id(
                    waveSimulation, chargerId);
                return current != nullptr &&
                       current->behaviorState == EnemyBehaviorState::Charge;
            },
            80));

        ranged = first_alive_entity(waveSimulation, EntityKind::Ranged);
        QVERIFY(ranged != nullptr);
        const int rangedId = ranged->id;
        QVERIFY(move_near_entity(waveSimulation, rangedId, 280.0f, 80.0f));
        QVERIFY(step_until(
            waveSimulation,
            [&waveSimulation, rangedId]() {
                const EntityState* current = entity_by_id(
                    waveSimulation, rangedId);
                return current != nullptr &&
                       current->behaviorState ==
                           EnemyBehaviorState::RangedAttack;
            },
            100));
        QVERIFY(step_until(
            waveSimulation,
            [&waveSimulation]() {
                return !waveSimulation.projectiles().empty();
            },
            50));
        QVERIFY(waveSimulation.projectiles().front().visualVariant ==
                ActorVisualVariant::RangedGunner);
    }

    // 测试远程投射物命中地面玩家后扣血、增加受击修订并标记重击效果。
    void projectileHitsGroundedPlayer()
    {
        GameSimulation simulation;
        int rangedId = 0;
        QVERIFY2(prepare_isolated_ranged_enemy(simulation, rangedId),
                 "could not prepare an isolated ranged enemy");
        QVERIFY(entity_by_id(simulation, rangedId) != nullptr);

        const std::uint32_t previousMaximum = maximum_projectile_id(simulation);
        std::uint32_t projectileId = 0;
        QVERIFY2(wait_for_new_projectile(
                     simulation, previousMaximum, projectileId),
                 "ranged enemy did not spawn a projectile");
        const int initialHealth = simulation.entities().front().hp;
        const std::uint32_t initialRevision =
            simulation.entities().front().impactRevision;

        QVERIFY2(step_until(
            simulation,
            [&simulation, projectileId, initialRevision]() {
                return simulation.entities().front().impactRevision >
                           initialRevision ||
                       !projectile_exists(simulation, projectileId);
            },
            120),
            "projectile neither hit nor expired");
        QCOMPARE(simulation.entities().front().hp, initialHealth - 8);
        QCOMPARE(simulation.entities().front().impactRevision,
                 initialRevision + 1u);
        QVERIFY(simulation.entities().front().lastImpact == ImpactLevel::Heavy);
    }

    // 测试投射物发射后立即跳跃可以避开按地面轨迹瞄准的攻击。
    void jumpAtProjectileLaunchAvoidsGroundTrajectory()
    {
        GameSimulation simulation;
        int rangedId = 0;
        QVERIFY2(prepare_isolated_ranged_enemy(simulation, rangedId),
                 "could not prepare an isolated ranged enemy");

        const std::uint32_t previousMaximum = maximum_projectile_id(simulation);
        std::uint32_t projectileId = 0;
        QVERIFY2(wait_for_new_projectile(
                     simulation, previousMaximum, projectileId),
                 "ranged enemy did not spawn a projectile");
        const int initialHealth = simulation.entities().front().hp;
        const std::uint32_t initialRevision =
            simulation.entities().front().impactRevision;
        QVERIFY(simulation.request_player_action(PlayerActionType::Jump));

        QVERIFY2(step_until(
            simulation,
            [&simulation, projectileId]() {
                return !projectile_exists(simulation, projectileId);
            },
            120),
            "ground-aimed projectile did not leave the simulation");
        QCOMPARE(simulation.entities().front().hp, initialHealth);
        QCOMPARE(simulation.entities().front().impactRevision,
                 initialRevision);
    }

    // 测试巡逻敌人掉生命补给、远程敌人掉能量补给，以及补给可被拾取移除。
    void defeatedEnemiesDropCollectiblePickups()
    {
        GameSimulation healthSimulation;
        const int patrollerId = healthSimulation.entities().at(1).id;
        QVERIFY2(defeat_entity(
                     healthSimulation, patrollerId, 5000, true),
                 "could not defeat a patroller at pickup-safe range");
        QVERIFY(!healthSimulation.pickups().empty());
        QVERIFY(healthSimulation.pickups().front().kind == PickupKind::Health);
        const std::uint32_t healthPickupId =
            healthSimulation.pickups().front().id;
        const auto healthPickupPosition =
            healthSimulation.pickups().front().position;
        QVERIFY(move_player_to(
            healthSimulation, healthPickupPosition.x,
            healthPickupPosition.laneY));
        healthSimulation.step(kStep);
        QVERIFY(std::none_of(
            healthSimulation.pickups().begin(),
            healthSimulation.pickups().end(),
            [healthPickupId](const PickupVm& pickup) {
                return pickup.id == healthPickupId;
            }));

        GameSimulation energySimulation;
        int rangedId = 0;
        QVERIFY2(prepare_isolated_ranged_enemy(energySimulation, rangedId),
                 "could not prepare a ranged enemy for its drop");
        QVERIFY2(defeat_entity(
                     energySimulation, rangedId, 6000, true),
                 "could not defeat the ranged enemy");
        QVERIFY(std::any_of(
            energySimulation.pickups().begin(),
            energySimulation.pickups().end(),
            [](const PickupVm& pickup) {
                return pickup.kind == PickupKind::Energy;
            }));
    }

    // 测试三波门禁、远程敌人变体、Boss 开场/战斗和胜利就绪的完整关卡流程。
    void wavesGatesBossEncounterAndVictory()
    {
        GameSimulation simulation;
        simulation.step(kStep);
        QVERIFY(simulation.encounter_state().currentWave == 1u);
        QVERIFY(simulation.encounter_state().totalWaves == 3u);

        simulation.set_move_right(true);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return nearly_equal(simulation.entities().front().pos.x, 760.0f);
            },
            400));
        simulation.clear_movement_input();
        QVERIFY(defeat_current_regular_wave(simulation));
        QVERIFY(wait_for_wave(simulation, 2));

        simulation.set_move_right(true);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return nearly_equal(simulation.entities().front().pos.x, 1540.0f);
            },
            500));
        simulation.clear_movement_input();
        const EntityState* waveTwoRanged = first_alive_entity(
            simulation, EntityKind::Ranged);
        QVERIFY(waveTwoRanged != nullptr);
        QVERIFY(waveTwoRanged->visualVariant ==
                ActorVisualVariant::RangedGunner);
        QVERIFY(defeat_current_regular_wave(simulation));
        QVERIFY(wait_for_wave(simulation, 3));

        simulation.set_move_right(true);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return nearly_equal(simulation.entities().front().pos.x, 2260.0f);
            },
            500));
        simulation.clear_movement_input();
        const EntityState* waveThreeRanged = first_alive_entity(
            simulation, EntityKind::Ranged);
        QVERIFY(waveThreeRanged != nullptr);
        QVERIFY(waveThreeRanged->visualVariant ==
                ActorVisualVariant::RangedRobot);
        QVERIFY(defeat_current_regular_wave(simulation));
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return simulation.encounter_state().phase ==
                       EncounterPhase::Cleared;
            },
            150));

        simulation.set_move_right(true);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return nearly_equal(simulation.entities().front().pos.x, 2350.0f);
            },
            100));
        simulation.clear_movement_input();
        QVERIFY(simulation.encounter_state().kind == EncounterKind::Boss);
        QVERIFY(simulation.encounter_state().phase == EncounterPhase::Intro);
        QVERIFY(first_alive_entity(simulation, EntityKind::Boss) == nullptr);

        simulation.step(1.0f);
        QVERIFY(first_alive_entity(simulation, EntityKind::Boss) == nullptr);
        QVERIFY(step_until(
            simulation,
            [&simulation]() {
                return first_alive_entity(simulation, EntityKind::Boss) != nullptr;
            },
            150));
        const EntityState* boss = first_alive_entity(
            simulation, EntityKind::Boss);
        QVERIFY(boss != nullptr);
        QCOMPARE(boss->maxHp, 140);
        const int bossId = boss->id;
        QVERIFY(simulation.encounter_state().kind == EncounterKind::Boss);
        QVERIFY(simulation.encounter_state().phase == EncounterPhase::Fighting);

        QVERIFY(move_near_entity(simulation, bossId, 100.0f, 50.0f));
        QVERIFY(step_until(
            simulation,
            [&simulation, bossId]() {
                const EntityState* current = entity_by_id(simulation, bossId);
                return current != nullptr && current->alive &&
                       current->behaviorState == EnemyBehaviorState::Charge;
            },
            100));
        QVERIFY2(defeat_entity(simulation, bossId, 10000),
                 "could not defeat the boss through legal attacks");
        QVERIFY(simulation.encounter_state().phase == EncounterPhase::Cleared);
        QVERIFY(!simulation.boss_victory_ready());
        QVERIFY(step_until(
            simulation,
            [&simulation]() { return simulation.boss_victory_ready(); },
            100));
        QVERIFY(simulation.boss_victory_ready());
    }
};

} // namespace alleyfist::viewmodel

int main(int argc, char** argv)
{
    // ViewModel 测试不创建界面，只用 QCoreApplication 驱动 QtTest。
    QCoreApplication application(argc, argv);
    alleyfist::viewmodel::ViewModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ViewModelTest.moc"
