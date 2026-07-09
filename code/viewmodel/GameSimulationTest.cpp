#include "GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

using namespace alleyfist;

struct TestRunner {
    int failures = 0;

    void expect(bool condition, const char* message)
    {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }
};

void input(GameSimulation& sim, InputAction action, ButtonState state)
{
    sim.handle_command(GameCommand::input_command(action, state));
}

void step(GameSimulation& sim, int frames, std::uint64_t& frame)
{
    for (int i = 0; i < frames; ++i) {
        ++frame;
        sim.step(1.0f / 60.0f, frame);
    }
}

void start_game(GameSimulation& sim, std::uint64_t& frame)
{
    input(sim, InputAction::Confirm, ButtonState::Triggered);
    step(sim, 1, frame);
}

void trigger_grunt_encounter(GameSimulation& sim, std::uint64_t& frame)
{
    input(sim, InputAction::MoveRight, ButtonState::Pressed);
    for (int i = 0; i < 180 && sim.snapshot().enemies.empty(); ++i) {
        step(sim, 1, frame);
    }
    input(sim, InputAction::MoveRight, ButtonState::Released);
    step(sim, 1, frame);
}

void heavy_attack(GameSimulation& sim, std::uint64_t& frame)
{
    input(sim, InputAction::HeavyAttack, ButtonState::Triggered);
    step(sim, 18, frame);
}

} // namespace

int main()
{
    TestRunner test;

    {
        GameSimulation sim;
        std::uint64_t frame = 0;

        test.expect(sim.snapshot().phase == GamePhase::Title, "simulation starts on title screen");
        const float startX = sim.snapshot().player.position.x;
        input(sim, InputAction::MoveRight, ButtonState::Pressed);
        step(sim, 20, frame);
        test.expect(std::abs(sim.snapshot().player.position.x - startX) < 0.01f,
                    "movement is ignored before start");

        start_game(sim, frame);
        test.expect(sim.snapshot().phase == GamePhase::Playing, "enter starts gameplay");

        input(sim, InputAction::Pause, ButtonState::Triggered);
        step(sim, 1, frame);
        test.expect(sim.snapshot().phase == GamePhase::Paused, "pause command pauses gameplay");

        const float pausedX = sim.snapshot().player.position.x;
        step(sim, 20, frame);
        test.expect(std::abs(sim.snapshot().player.position.x - pausedX) < 0.01f,
                    "simulation does not move while paused");

        input(sim, InputAction::Pause, ButtonState::Triggered);
        step(sim, 1, frame);
        test.expect(sim.snapshot().phase == GamePhase::Playing, "pause command resumes gameplay");
    }

    {
        GameSimulation sim;
        std::uint64_t frame = 0;
        start_game(sim, frame);

        input(sim, InputAction::Jump, ButtonState::Triggered);
        step(sim, 8, frame);
        test.expect(sim.snapshot().player.position.z > 1.0f, "jump raises player off the ground");
        test.expect(sim.snapshot().player.state == ActorState::Jump, "jump state is visible");

        step(sim, 45, frame);
        test.expect(sim.snapshot().player.position.z == 0.0f, "jump lands back on ground");
    }

    {
        GameSimulation sim;
        std::uint64_t frame = 0;
        start_game(sim, frame);
        trigger_grunt_encounter(sim, frame);

        test.expect(sim.snapshot().phase == GamePhase::EncounterLocked, "grunt encounter locks the game phase");
        test.expect(sim.snapshot().progress.activeEncounterId == 1, "grunt encounter becomes active");
        test.expect(sim.snapshot().enemies.size() == 3, "grunt encounter spawns three enemies");

        const float rightBoundary = sim.snapshot().map.rightBoundaryX;
        input(sim, InputAction::MoveRight, ButtonState::Pressed);
        step(sim, 180, frame);
        input(sim, InputAction::MoveRight, ButtonState::Released);
        step(sim, 1, frame);
        test.expect(sim.snapshot().player.position.x <= rightBoundary + 0.01f,
                    "encounter lock prevents running past the fight");
    }

    {
        GameSimulation sim;
        std::uint64_t frame = 0;
        start_game(sim, frame);
        trigger_grunt_encounter(sim, frame);

        const auto healthBefore = sim.snapshot().enemies.front().health.current;
        input(sim, InputAction::HeavyAttack, ButtonState::Triggered);
        step(sim, 1, frame);

        const bool anyDamaged = std::any_of(sim.snapshot().enemies.begin(),
                                            sim.snapshot().enemies.end(),
                                            [healthBefore](const ActorViewData& enemy) {
                                                return enemy.health.current < healthBefore;
                                            });
        test.expect(anyDamaged, "heavy attack damages a nearby enemy");
        test.expect(sim.snapshot().player.state == ActorState::HeavyAttack,
                    "heavy attack state persists long enough to render");
    }

    {
        GameSimulation sim;
        std::uint64_t frame = 0;
        start_game(sim, frame);
        trigger_grunt_encounter(sim, frame);

        for (int i = 0; i < 12 && sim.snapshot().progress.activeEncounterId != kInvalidEncounterId; ++i) {
            heavy_attack(sim, frame);
            step(sim, 20, frame);
        }

        test.expect(sim.snapshot().progress.activeEncounterId == kInvalidEncounterId,
                    "defeating all grunts clears the encounter");
        test.expect(sim.snapshot().phase == GamePhase::ClearToGo,
                    "cleared grunt encounter switches to clear-to-go phase");
        test.expect(sim.snapshot().map.showGoIndicator, "cleared encounter enables GO indicator");
        test.expect(sim.snapshot().result.defeatedEnemies >= 3, "defeated enemies are counted");

        input(sim, InputAction::MoveRight, ButtonState::Pressed);
        for (int i = 0; i < 900 && !sim.snapshot().progress.bossSpawned; ++i) {
            step(sim, 1, frame);
        }
        input(sim, InputAction::MoveRight, ButtonState::Released);
        step(sim, 1, frame);

        test.expect(sim.snapshot().progress.bossSpawned, "boss encounter spawns near level end");
        test.expect(sim.snapshot().hud.showBossHealth, "boss health is exposed to HUD");

        for (int i = 0; i < 12 && sim.snapshot().phase != GamePhase::Win; ++i) {
            heavy_attack(sim, frame);
            step(sim, 25, frame);
        }

        test.expect(sim.snapshot().phase == GamePhase::Win, "defeating the boss wins the game");
        test.expect(sim.snapshot().result.winReason == WinReason::BossDefeated,
                    "win result records boss defeat");
    }

    return test.failures == 0 ? 0 : 1;
}
