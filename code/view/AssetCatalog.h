#pragma once

#include "../common/game_state.h"

#include <QPixmap>
#include <QString>

#include <array>

namespace alleyfist::view {

struct SpriteClip {
    QPixmap sheet;
    int frameCount = 0;
    float framesPerSecond = 1.0f;
    bool looping = false;
};

struct ActorArtSet {
    SpriteClip idle;
    SpriteClip walk;
    SpriteClip lightAttack;
    SpriteClip heavyAttack;
    SpriteClip rangedAttack;
    SpriteClip ambush;
    SpriteClip charge;
    SpriteClip jump;
    SpriteClip airAttack;
    SpriteClip hurt;
    SpriteClip dead;
    Facing sourceFacing = Facing::Right;
    float horizontalPivot = 0.5f;
};

/// Owns the immutable Qt resources used by the view.
struct AssetCatalog {
    AssetCatalog();

    const ActorArtSet* actor_art_set(const ActorState& actor) const noexcept;
    const SpriteClip* actor_clip(const ActorState& actor) const noexcept;

    ActorArtSet playerArt;
    ActorArtSet patrollerArt;
    ActorArtSet ambusherArt;
    ActorArtSet chargerArt;
    ActorArtSet rangedGunnerArt;
    ActorArtSet rangedRobotArt;
    ActorArtSet bossArt;
    QPixmap actorShadow;
    QPixmap robotProjectile;
    QPixmap robotProjectileAlt;
    QPixmap stageTileset;
    QPixmap stageBack;
    QPixmap stageFore;
    QPixmap stageCar;
    QPixmap stageBarrel;
    QPixmap stageHydrant;
    std::array<QPixmap, 8> healthBarArt;
    std::array<QPixmap, 8> energyBarArt;
    std::array<QPixmap, 4> hudScrollArt;
    std::array<QPixmap, 9> interfaceFrameArt;
    std::array<QPixmap, 3> interfaceLogoArt;
    std::array<QPixmap, 6> redButtonArt;
    std::array<QPixmap, 6> cyanButtonArt;
    std::array<QPixmap, 6> greenButtonArt;
    QPixmap startIcon;
    QPixmap pauseIcon;
    QPixmap bossIcon;
    QPixmap winIcon;
    QPixmap lossIcon;
    QPixmap restartIcon;
    QString interfaceFontFamily = QStringLiteral("Arial");
};

} // namespace alleyfist::view
