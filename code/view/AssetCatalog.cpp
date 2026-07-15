#include "AssetCatalog.h"

#include <QFontDatabase>
#include <QtResource>

#include <cstddef>

static void ensureArtResourcesInitialized()
{
    static const bool initialized = [] {
        Q_INIT_RESOURCE(alleyfist_art);
        return true;
    }();
    (void)initialized;
}

namespace alleyfist::view {

AssetCatalog::AssetCatalog()
{
    ensureArtResourcesInitialized();

    const auto clip = [](const char* path, int frames, float fps,
                         bool looping = false) {
        return SpriteClip{QPixmap(path), frames, fps, looping};
    };

    playerArt.idle = clip(":/art/Spritesheets/Brawler Girl/idle.png", 4, 6.0f, true);
    playerArt.walk = clip(":/art/Spritesheets/Brawler Girl/walk.png", 10, 12.0f, true);
    playerArt.lightAttack = clip(":/art/Spritesheets/Brawler Girl/jab.png", 3, 16.0f);
    playerArt.heavyAttack = clip(":/art/Spritesheets/Brawler Girl/kick.png", 5, 18.0f);
    playerArt.jump = clip(":/art/Spritesheets/Brawler Girl/jump.png", 4, 7.0f);
    playerArt.airAttack = clip(":/art/Spritesheets/Brawler Girl/jump_kick.png", 3, 14.0f);
    playerArt.hurt = clip(":/art/Spritesheets/Brawler Girl/hurt.png", 2, 14.0f);
    playerArt.sourceFacing = Facing::Right;

    patrollerArt.idle = clip(":/art/Spritesheets/Enemy Punk/idle.png", 4, 6.0f, true);
    patrollerArt.walk = clip(":/art/Spritesheets/Enemy Punk/walk.png", 4, 8.0f, true);
    patrollerArt.lightAttack = clip(":/art/Spritesheets/Enemy Punk/punch.png", 3, 16.0f);
    patrollerArt.heavyAttack = patrollerArt.lightAttack;
    patrollerArt.hurt = clip(":/art/Spritesheets/Enemy Punk/hurt.png", 4, 14.0f);
    patrollerArt.dead = clip(":/art/Spritesheets/Enemy Punk/death.png", 4, 6.0f);
    patrollerArt.sourceFacing = Facing::Left;

    ambusherArt.idle = clip(":/art/伏击型飞行敌人/Idle.png", 4, 6.0f, true);
    ambusherArt.walk = clip(":/art/伏击型飞行敌人/Walk.png", 4, 8.0f, true);
    ambusherArt.ambush = clip(":/art/伏击型飞行敌人/Attack.png", 4, 8.0f);
    ambusherArt.hurt = clip(":/art/伏击型飞行敌人/Hurt.png", 2, 14.0f);
    ambusherArt.dead = clip(":/art/伏击型飞行敌人/Death.png", 4, 8.0f);
    ambusherArt.sourceFacing = Facing::Right;
    ambusherArt.horizontalPivot = 1.0f / 3.0f;

    chargerArt.idle = clip(":/art/冲撞型敌人/Idle.png", 4, 6.0f, true);
    chargerArt.walk = clip(":/art/冲撞型敌人/Walk.png", 6, 9.0f, true);
    chargerArt.charge = clip(":/art/冲撞型敌人/Attack.png", 4, 8.0f);
    chargerArt.hurt = clip(":/art/冲撞型敌人/Hurt.png", 2, 14.0f);
    chargerArt.dead = clip(":/art/冲撞型敌人/Death.png", 6, 8.0f);
    chargerArt.sourceFacing = Facing::Right;
    chargerArt.horizontalPivot = 1.0f / 3.0f;

    rangedGunnerArt.idle = clip(":/art/远程型敌人/Idle.png", 4, 6.0f, true);
    rangedGunnerArt.walk = clip(":/art/远程型敌人/Walk.png", 6, 9.0f, true);
    rangedGunnerArt.rangedAttack = clip(":/art/远程型敌人/Attack.png", 9, 28.0f);
    rangedGunnerArt.hurt = clip(":/art/远程型敌人/Hurt.png", 2, 14.0f);
    rangedGunnerArt.dead = clip(":/art/远程型敌人/Death.png", 6, 8.0f);
    rangedGunnerArt.sourceFacing = Facing::Right;
    rangedGunnerArt.horizontalPivot = 1.0f / 3.0f;

    rangedRobotArt.idle = clip(":/art/远程型敌人2（机器人）/Idle.png", 4, 6.0f, true);
    rangedRobotArt.walk = clip(":/art/远程型敌人2（机器人）/Walk.png", 4, 8.0f, true);
    rangedRobotArt.rangedAttack = clip(":/art/远程型敌人2（机器人）/Attack.png", 4, 14.0f);
    rangedRobotArt.hurt = clip(":/art/远程型敌人2（机器人）/Hurt.png", 2, 14.0f);
    rangedRobotArt.dead = clip(":/art/远程型敌人2（机器人）/Death.png", 4, 6.0f);
    rangedRobotArt.sourceFacing = Facing::Right;
    rangedRobotArt.horizontalPivot = 1.0f / 3.0f;

    bossArt.idle = clip(":/art/Boss/Idle.png", 4, 5.0f, true);
    bossArt.walk = clip(":/art/Boss/Walk.png", 6, 9.0f, true);
    bossArt.lightAttack = clip(":/art/Boss/Attack1.png", 8, 12.0f);
    bossArt.heavyAttack = clip(":/art/Boss/Attack2.png", 6, 10.0f);
    bossArt.rangedAttack = clip(":/art/Boss/Attack3.png", 4, 8.0f);
    bossArt.charge = bossArt.heavyAttack;
    bossArt.airAttack = clip(":/art/Boss/Attack4.png", 6, 10.0f);
    bossArt.ambush = clip(":/art/Boss/Sneer.png", 6, 8.0f);
    bossArt.hurt = clip(":/art/Boss/Hurt.png", 2, 14.0f);
    bossArt.dead = clip(":/art/Boss/Death.png", 6, 8.0f);
    bossArt.sourceFacing = Facing::Right;

    actorShadow = QPixmap(":/art/Sprites/shadow.png");
    robotProjectile = QPixmap(":/art/远程型敌人2（机器人）/Ball1.png");
    robotProjectileAlt = QPixmap(":/art/远程型敌人2（机器人）/Ball2.png");
    stageTileset = QPixmap(":/art/Stage Layers/tileset.png");
    stageBack = QPixmap(":/art/Stage Layers/back.png");
    stageFore = QPixmap(":/art/Stage Layers/fore.png");
    stageCar = QPixmap(":/art/Stage Layers/props/car.png");
    stageBarrel = QPixmap(":/art/Stage Layers/props/barrel.png");
    stageHydrant = QPixmap(":/art/Stage Layers/props/hydrant.png");

    for (std::size_t i = 0; i < healthBarArt.size(); ++i) {
        const auto suffix = QString::number(static_cast<int>(i + 1));
        healthBarArt[i] = QPixmap(":/art/HUD/HealthBar" + suffix + ".png");
        energyBarArt[i] = QPixmap(":/art/HUD/EnergyBar" + suffix + ".png");
    }
    for (std::size_t i = 0; i < hudScrollArt.size(); ++i) {
        const auto suffix = QString::number(static_cast<int>(i + 1));
        hudScrollArt[i] = QPixmap(":/art/HUD/Scrolling" + suffix + ".png");
    }

    interfaceFrameArt = {
        QPixmap(":/art/界面图/1 Frames/Frame_01.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_02.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_03.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_10.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_11.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_12.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_19.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_20.png"),
        QPixmap(":/art/界面图/1 Frames/Frame_21.png"),
    };
    interfaceLogoArt = {
        QPixmap(":/art/界面图/5 Logo/Logo1.png"),
        QPixmap(":/art/界面图/5 Logo/Logo2.png"),
        QPixmap(":/art/界面图/5 Logo/Logo3.png"),
    };

    const auto loadButton = [](const QString& set) {
        std::array<QPixmap, 6> art;
        constexpr const char* suffixes[] = {"01", "02", "03", "05", "06", "07"};
        for (std::size_t i = 0; i < art.size(); ++i) {
            art[i] = QPixmap(QString(":/art/界面图/6 Buttons/%1/%1_%2.png")
                                 .arg(set)
                                 .arg(suffixes[i]));
        }
        return art;
    };
    redButtonArt = loadButton("1");
    cyanButtonArt = loadButton("2");
    greenButtonArt = loadButton("7");

    startIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_20.png");
    pauseIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_05.png");
    bossIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_10.png");
    winIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_02.png");
    lossIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_19.png");
    restartIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_17.png");

    const int fontId = QFontDatabase::addApplicationFont(
        ":/art/界面图/10 Font/CyberpunkCraftpixPixel.otf");
    if (fontId >= 0) {
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) interfaceFontFamily = families.front();
    }
}

const ActorArtSet* AssetCatalog::actor_art_set(const ActorState& actor) const noexcept
{
    if (actor.team == Team::Player) return &playerArt;

    switch (actor.kind) {
    case ActorKind::Patroller: return &patrollerArt;
    case ActorKind::Ambusher:  return &ambusherArt;
    case ActorKind::Charger:   return &chargerArt;
    case ActorKind::Ranged:
        return actor.visualVariant == ActorVisualVariant::RangedRobot
                   ? &rangedRobotArt
                   : &rangedGunnerArt;
    case ActorKind::Boss:   return &bossArt;
    case ActorKind::Player: return &playerArt;
    }
    return nullptr;
}

const SpriteClip* AssetCatalog::actor_clip(const ActorState& actor) const noexcept
{
    const auto* art = actor_art_set(actor);
    if (art == nullptr) return nullptr;

    switch (actor.actionState) {
    case ActorActionState::Idle:         return &art->idle;
    case ActorActionState::Walk:         return &art->walk;
    case ActorActionState::LightAttack:  return &art->lightAttack;
    case ActorActionState::HeavyAttack:  return &art->heavyAttack;
    case ActorActionState::RangedAttack: return &art->rangedAttack;
    case ActorActionState::Ambush:       return &art->ambush;
    case ActorActionState::Charge:       return &art->charge;
    case ActorActionState::Jump:         return &art->jump;
    case ActorActionState::AirAttack:    return &art->airAttack;
    case ActorActionState::Hurt:         return &art->hurt;
    case ActorActionState::Dead:         return &art->dead;
    }
    return nullptr;
}

} // namespace alleyfist::view
