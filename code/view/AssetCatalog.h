#pragma once

#include "../common/game_state.h"

#include <QPixmap>
#include <QString>

#include <array>

namespace alleyfist::view {

/// @brief 一条横向 spritesheet 的播放参数。
///
/// sheet 保存整张图，frameCount 表示水平切分帧数；View 根据动作状态和
/// PresentationState 的动画时钟选择当前帧。
struct SpriteClip {
    QPixmap sheet;
    int frameCount = 0;
    float framesPerSecond = 1.0f;
    bool looping = false;
};

/// @brief 某个角色类型的一组动作素材。
///
/// sourceFacing 记录素材原始朝向，绘制时如果 ActorState.facing 不一致就水平翻转。
/// horizontalPivot 用来修正不同素材的脚底/身体中心，避免攻击动画看起来漂移。
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

/// @brief View 层的不可变资源目录。
///
/// AssetCatalog 统一加载 Qt resource 中的角色、场景、HUD 和界面素材。
/// GameWidget 只按语义索取素材，不在绘制代码里散落路径字符串。
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
