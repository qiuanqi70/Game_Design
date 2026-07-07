#include "common/common.h"

#include <iostream>

int main()
{
    alleyfist::GameSnapshot snapshot;
    snapshot.phase = alleyfist::GamePhase::Playing;
    snapshot.player.id = alleyfist::kPlayerActorId;
    snapshot.player.kind = alleyfist::ActorKind::Player;
    snapshot.player.team = alleyfist::Team::Player;
    snapshot.hud.playerHealth = {100, 100};
    snapshot.hud.playerEnergy = {100, 100};

    std::cout << "AlleyFist foundation is ready." << std::endl;
    return 0;
}
