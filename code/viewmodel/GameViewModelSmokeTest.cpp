#include "GameViewModel.h"

#include <cassert>

int main()
{
    using namespace alleyfist;
    using namespace alleyfist::viewmodel;

    GameViewModel viewModel;
    viewModel.get_confirm_command()();

    auto moveRight = viewModel.get_move_right_command();
    moveRight(true);

    auto tick = viewModel.get_tick_command();
    for (int i = 0; i < 800; ++i) {
        tick(0.016f, 0u);
    }

    const GameState* state = viewModel.get_game_state();
    assert(state != nullptr);
    assert(state->encounter.kind == EncounterKind::Boss);
    assert(state->encounter.phase == EncounterPhase::Intro || state->encounter.phase == EncounterPhase::Fighting);
    assert(state->hud.showBossHealth || state->encounter.kind != EncounterKind::Boss);

    return 0;
}
