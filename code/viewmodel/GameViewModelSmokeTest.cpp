#include "GameViewModel.h"

#include <cassert>

int main()
{
    using namespace alleyfist;
    using namespace alleyfist::viewmodel;

    {
        GameSimulation sim;
        sim.start();

        for (auto& entity : const_cast<EntityList&>(sim.entities())) {
            if (entity.kind != EntityKind::Player) {
                entity.alive = false;
            }
        }

        auto& player = const_cast<EntityState&>(sim.entities()[0]);
        player.pos.x = 2350.0f;

        for (int i = 0; i < 100; ++i) {
            sim.step(0.016f);
        }

        assert(sim.boss_spawned());
        assert(sim.encounter_state().kind == EncounterKind::Boss);
    }

    {
        GameViewModel viewModel;
        viewModel.get_confirm_command()();

        auto& firstEnemy = const_cast<EntityState&>(viewModel.entities().at(1));
        firstEnemy.behaviorState = EnemyBehaviorState::RangedAttack;
        auto tick = viewModel.get_tick_command();
        tick(0.016f, 0u);

        const GameState* state = viewModel.get_game_state();
        assert(state != nullptr);
        assert(!state->enemies.empty());
        assert(state->enemies.front().actionState == ActorActionState::Walk || state->enemies.front().actionState == ActorActionState::RangedAttack);
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
            if (entity.id != 0) {
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

        auto& player = const_cast<EntityState&>(sim.entities()[0]);
        sim.player_move(5000.0f, 0.0f);
        assert(player.pos.x <= 3000.0f);
        assert(player.pos.x >= 0.0f);
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

    return 0;
}



