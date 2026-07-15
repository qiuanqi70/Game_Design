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

constexpr float kStep = 0.02f;

bool nearly_equal(float first, float second, float tolerance = 0.001f)
{
    return std::abs(first - second) <= tolerance;
}

const EntityState* entity_by_id(const GameSimulation& simulation, int id)
{
    const auto& entities = simulation.entities();
    const auto found = std::find_if(
        entities.begin(), entities.end(), [id](const EntityState& entity) {
            return entity.id == id;
        });
    return found == entities.end() ? nullptr : &*found;
}

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

std::uint32_t alive_regular_enemy_count(const GameSimulation& simulation)
{
    return static_cast<std::uint32_t>(std::count_if(
        simulation.entities().begin(), simulation.entities().end(),
        [](const EntityState& entity) {
            return entity.alive && entity.kind != EntityKind::Player &&
                   entity.kind != EntityKind::Boss;
        }));
}

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

void step_for(GameSimulation& simulation, float seconds, float dt = kStep)
{
    float remaining = seconds;
    while (remaining > 0.0f) {
        const float currentStep = std::min(dt, remaining);
        simulation.step(currentStep);
        remaining -= currentStep;
    }
}

void set_movement_towards(GameSimulation& simulation, float dx, float dy,
                          float toleranceX = 2.0f,
                          float toleranceY = 2.0f)
{
    simulation.set_move_left(dx < -toleranceX);
    simulation.set_move_right(dx > toleranceX);
    simulation.set_move_up(dy < -toleranceY);
    simulation.set_move_down(dy > toleranceY);
}

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

std::uint32_t maximum_projectile_id(const GameSimulation& simulation)
{
    std::uint32_t maximum = 0;
    for (const auto& projectile : simulation.projectiles()) {
        maximum = std::max(maximum, projectile.id);
    }
    return maximum;
}

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
    QCoreApplication application(argc, argv);
    alleyfist::viewmodel::ViewModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ViewModelTest.moc"
