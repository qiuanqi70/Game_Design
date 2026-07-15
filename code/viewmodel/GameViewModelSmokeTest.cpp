#include "GameViewModel.h"

#include <cassert>

namespace {

using alleyfist::viewmodel::EntityKind;
using alleyfist::viewmodel::EntityList;
using alleyfist::viewmodel::GameSimulation;
using alleyfist::viewmodel::GameViewModel;

void defeat_active_regular_enemies(EntityList& entities)
{
    for (auto& entity : entities) {
        if (entity.kind != EntityKind::Player && entity.kind != EntityKind::Boss) {
            entity.alive = false;
        }
    }
}

void complete_regular_waves(GameSimulation& simulation)
{
    for (int wave = 0; wave < 3; ++wave) {
        auto& entities = const_cast<EntityList&>(simulation.entities());
        defeat_active_regular_enemies(entities);
        simulation.step(1.01f);
    }
}

void complete_regular_waves(GameViewModel& viewModel)
{
    auto tick = viewModel.get_tick_command();
    for (int wave = 0; wave < 3; ++wave) {
        auto& entities = const_cast<EntityList&>(viewModel.entities());
        defeat_active_regular_enemies(entities);
        tick(1.01f, 0u);
    }
}

} // namespace

int main()
{
    using namespace alleyfist;
    using namespace alleyfist::viewmodel;

    {
        GameSimulation sim;
        sim.start();

        complete_regular_waves(sim);
        sim.player_move(5000.0f, 0.0f);
        auto& player = const_cast<EntityState&>(sim.entities()[0]);
        assert(player.pos.x == 2350.0f);

        sim.step(1.60f);

        assert(!sim.boss_spawned());
        assert(sim.encounter_state().phase == EncounterPhase::Intro);

        sim.player_move(-5000.0f, 0.0f);
        assert(player.pos.x == 2180.0f);
        sim.step(1.21f);

        assert(sim.boss_spawned());
        assert(sim.encounter_state().kind == EncounterKind::Boss);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();

        auto& firstEnemy = const_cast<EntityState&>(viewModel.entities().at(1));
        firstEnemy.behaviorState = EnemyBehaviorState::Idle;
        auto tick = viewModel.get_tick_command();
        tick(0.016f, 0u);

        const GameState* state = viewModel.get_game_state();
        assert(state != nullptr);
        assert(!state->enemies.empty());
        assert(state->enemies.front().actionState == ActorActionState::Idle);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();
        viewModel.get_state_toggle_command()();
        viewModel.get_primary_action_command()();

        assert(viewModel.get_game_state()->player.actionState == ActorActionState::AirAttack);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        auto& player = entities.front();
        auto& enemy = entities.at(1);
        enemy.pos = player.pos;
        const int initialHealth = player.hp;

        sim.step(0.016f);
        assert(player.hp == initialHealth);
        assert(enemy.behaviorState == EnemyBehaviorState::MeleeAttack);

        for (int i = 0; i < 12; ++i) {
            sim.step(0.02f);
        }

        assert(player.hp == initialHealth - 1);
        const std::uint32_t impactRevision = player.impactRevision;

        for (int i = 0; i < 10; ++i) {
            sim.step(0.02f);
        }

        assert(player.hp == initialHealth - 1);
        assert(player.impactRevision == impactRevision);

        for (std::size_t i = 1; i < entities.size(); ++i) {
            entities[i].alive = false;
        }
        for (int i = 0; i < 20; ++i) {
            sim.step(0.02f);
        }

        assert(player.hurtTimer == 0.0f);
        assert(player.behaviorState != EnemyBehaviorState::Hurt);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        auto& player = entities.front();
        auto& enemy = entities.at(1);
        enemy.pos = player.pos;
        const int initialHealth = player.hp;

        sim.step(0.016f);
        player.pos.x += 100.0f;
        for (int i = 0; i < 24; ++i) {
            sim.step(0.02f);
        }

        assert(player.hp == initialHealth);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        auto& player = entities.front();
        for (std::size_t i = 1; i < entities.size(); ++i) {
            entities[i].alive = i <= 2;
            entities[i].pos = player.pos;
            entities[i].behaviorState = EnemyBehaviorState::MeleeAttack;
            entities[i].attackTimer = 0.16f;
            entities[i].attackCooldown = 1.0f;
            entities[i].attackHitApplied = false;
        }

        sim.step(0.016f);

        assert(player.hp == 99);
        assert(player.impactRevision == 1u);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();

        auto& entities = const_cast<EntityList&>(viewModel.entities());
        entities.at(1).pos = entities.front().pos;
        viewModel.get_tick_command()(0.016f, 0u);

        assert(viewModel.get_game_state()->player.health.current == 100);
        assert(viewModel.get_game_state()->enemies.front().actionState == ActorActionState::LightAttack);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        auto& player = entities.front();
        auto& enemy = entities.at(1);
        player.hp = 80;
        enemy.hp = 10;
        enemy.pos = player.pos;
        const std::uint32_t impactRevision = player.impactRevision;

        sim.player_attack();
        sim.step(0.016f);

        assert(player.hp == 100);
        assert(player.impactRevision == impactRevision);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& firstEnemy = const_cast<EntityState&>(sim.entities().at(1));
        firstEnemy.hp = 0;
        firstEnemy.alive = true;
        firstEnemy.pickupDropped = false;

        sim.step(0.016f);
        assert(sim.pickups().size() >= 1u);
    }

    {
        GameSimulation sim;
        sim.start();
        assert(sim.entities().size() == 5u);

        auto count_alive_enemies = [&sim]() {
            std::uint32_t count = 0;
            for (std::size_t i = 1; i < sim.entities().size(); ++i) {
                if (sim.entities()[i].alive) ++count;
            }
            return count;
        };

        assert(count_alive_enemies() == 4u);

        for (auto& entity : const_cast<EntityList&>(sim.entities())) {
            if (entity.kind != EntityKind::Player) {
                entity.alive = false;
            }
        }

        assert(count_alive_enemies() == 0u);

        for (int i = 0; i < 100; ++i) {
            sim.step(0.016f);
        }

        assert(sim.entities().size() > 5u);
        assert(count_alive_enemies() > 0u);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 760.0f);

        entities.front().pos.x = 2350.0f;
        sim.step(3.0f);
        assert(!sim.boss_spawned());
        sim.player_move(0.0f, 0.0f);
        assert(entities.front().pos.x == 760.0f);

        defeat_active_regular_enemies(entities);
        sim.step(0.99f);
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 760.0f);
        assert(sim.encounter_state().phase == EncounterPhase::Intro);

        sim.step(0.02f);
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 1540.0f);
        assert(sim.encounter_state().currentWave == 2u);

        defeat_active_regular_enemies(entities);
        sim.step(1.01f);
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 2260.0f);
        assert(sim.encounter_state().currentWave == 3u);

        defeat_active_regular_enemies(entities);
        sim.step(0.99f);
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 2260.0f);

        sim.step(0.02f);
        sim.player_move(5000.0f, 0.0f);
        assert(entities.front().pos.x == 2350.0f);
        assert(sim.encounter_state().phase == EncounterPhase::Cleared);

        sim.reset();
        sim.player_move(5000.0f, 0.0f);
        assert(sim.entities().front().pos.x == 760.0f);
    }

    {
        GameSimulation sim;
        sim.start();

        for (auto& entity : const_cast<EntityList&>(sim.entities())) {
            if (entity.kind != EntityKind::Player) {
                entity.alive = false;
            }
        }

        sim.step(0.016f);

        const auto& encounter = sim.encounter_state();
        assert(encounter.kind == EncounterKind::EnemyWave);
        assert(encounter.phase == EncounterPhase::Intro);
        assert(encounter.currentWave == 2);
        assert(encounter.totalWaves == 3);
        assert(encounter.remainingEnemies == 0);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& firstEnemy = const_cast<EntityState&>(sim.entities().at(1));
        firstEnemy.hp = 0;
        firstEnemy.alive = true;
        firstEnemy.pickupDropped = false;

        sim.step(0.016f);

        const auto& pickups = sim.pickups();
        assert(!pickups.empty());
        assert(pickups[0].kind == PickupKind::Health);
    }

    {
        GameSimulation sim;
        sim.start();

        auto& entities = const_cast<EntityList&>(sim.entities());
        for (std::size_t i = 1; i < entities.size(); ++i) {
            entities[i].alive = false;
        }
        sim.step(1.01f);

        EntityState* gunner = nullptr;
        for (auto& entity : entities) {
            if (entity.alive && entity.kind == EntityKind::Ranged) {
                gunner = &entity;
                break;
            }
        }
        assert(gunner != nullptr);
        assert(gunner->visualVariant == ActorVisualVariant::RangedGunner);

        gunner->pos.x = entities.front().pos.x + 200.0f;
        gunner->pos.laneY = entities.front().pos.laneY;
        gunner->behaviorState = EnemyBehaviorState::Idle;
        gunner->attackCooldown = 0.0f;
        sim.step(0.016f);
        sim.step(0.18f);

        bool foundGunnerProjectile = false;
        for (const auto& projectile : sim.projectiles()) {
            foundGunnerProjectile = foundGunnerProjectile ||
                                     projectile.visualVariant == ActorVisualVariant::RangedGunner;
            if (projectile.visualVariant == ActorVisualVariant::RangedGunner) {
                assert(projectile.position.z > 20.0f);
                assert(projectile.position.x < gunner->pos.x);
            }
        }
        assert(foundGunnerProjectile);

        for (std::size_t i = 1; i < entities.size(); ++i) {
            entities[i].alive = false;
        }
        sim.step(1.01f);

        EntityState* robot = nullptr;
        for (auto& entity : entities) {
            if (entity.alive && entity.kind == EntityKind::Ranged) {
                robot = &entity;
                break;
            }
        }
        assert(robot != nullptr);
        assert(robot->visualVariant == ActorVisualVariant::RangedRobot);

        robot->pos.x = entities.front().pos.x + 200.0f;
        robot->pos.laneY = entities.front().pos.laneY;
        robot->behaviorState = EnemyBehaviorState::Idle;
        robot->attackCooldown = 0.0f;
        sim.step(0.016f);
        sim.step(0.18f);

        bool foundRobotProjectile = false;
        for (const auto& projectile : sim.projectiles()) {
            foundRobotProjectile = foundRobotProjectile ||
                                    projectile.visualVariant == ActorVisualVariant::RangedRobot;
            if (projectile.visualVariant == ActorVisualVariant::RangedRobot) {
                assert(projectile.position.z > 20.0f);
                assert(projectile.position.x < robot->pos.x);
            }
        }
        assert(foundRobotProjectile);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();

        auto& entities = const_cast<EntityList&>(viewModel.entities());
        auto& enemy = entities.at(1);
        const std::uint32_t enemyId = static_cast<std::uint32_t>(enemy.id);
        enemy.hp = 10;
        enemy.pos = entities.front().pos;
        viewModel.get_primary_action_command()();

        const auto findEnemy = [&viewModel, enemyId]() -> const ActorState* {
            for (const auto& actor : viewModel.get_game_state()->enemies) {
                if (actor.id == enemyId) {
                    return &actor;
                }
            }
            return nullptr;
        };

        const ActorState* dyingEnemy = findEnemy();
        assert(dyingEnemy != nullptr);
        assert(dyingEnemy->visible);
        assert(dyingEnemy->actionState == ActorActionState::Dead);
        const Facing deathFacing = dyingEnemy->facing;

        viewModel.get_move_right_command()(true);
        viewModel.get_tick_command()(0.10f, 0u);
        viewModel.get_move_right_command()(false);
        dyingEnemy = findEnemy();
        assert(dyingEnemy != nullptr);
        assert(dyingEnemy->facing == deathFacing);

        viewModel.get_tick_command()(0.60f, 0u);
        dyingEnemy = findEnemy();
        assert(dyingEnemy != nullptr);
        assert(dyingEnemy->actionState == ActorActionState::Dead);

        viewModel.get_tick_command()(0.06f, 0u);
        assert(findEnemy() == nullptr);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();

        complete_regular_waves(viewModel);
        auto& entities = const_cast<EntityList&>(viewModel.entities());
        entities.front().pos.x = 2350.0f;
        viewModel.get_tick_command()(2.81f, 0u);

        EntityState* boss = nullptr;
        for (auto& entity : entities) {
            if (entity.kind == EntityKind::Boss) {
                boss = &entity;
                break;
            }
        }
        assert(boss != nullptr);
        const std::uint32_t bossId = static_cast<std::uint32_t>(boss->id);
        boss->hp = 10;
        boss->pos = entities.front().pos;
        viewModel.get_primary_action_command()();

        const auto findBoss = [&viewModel, bossId]() -> const ActorState* {
            for (const auto& actor : viewModel.get_game_state()->enemies) {
                if (actor.id == bossId) {
                    return &actor;
                }
            }
            return nullptr;
        };

        assert(viewModel.get_game_state()->phase == GamePhase::Playing);
        assert(findBoss() != nullptr);
        assert(findBoss()->actionState == ActorActionState::Dead);
        assert(viewModel.get_game_state()->encounter.phase == EncounterPhase::Cleared);
        assert(!viewModel.get_game_state()->hud.showBossHealth);

        viewModel.get_tick_command()(0.70f, 0u);
        assert(viewModel.get_game_state()->phase == GamePhase::Playing);
        assert(findBoss() != nullptr);
        assert(viewModel.get_game_state()->encounter.phase == EncounterPhase::Cleared);

        viewModel.get_tick_command()(0.06f, 0u);
        assert(viewModel.get_game_state()->phase == GamePhase::Win);
        assert(viewModel.get_game_state()->encounter.phase == EncounterPhase::Cleared);
        assert(findBoss() == nullptr);
    }

    return 0;
}
