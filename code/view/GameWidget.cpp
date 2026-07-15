#include "GameWidget.h"
#include "SoundManager.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

namespace alleyfist {

// GameWidget 是纯 View：这里可以出现 Qt 事件、QPainter、颜色和布局，
// 但不写 AI、碰撞、伤害、刷怪等规则；这些规则通过只读显示属性从 ViewModel 单向流入。

// ============================================================================
// 构造与生命周期
// ============================================================================

GameWidget::GameWidget(QWidget* parent)
    : QWidget(parent)
{
    // 游戏画布背景为黑色，焦点用于接收键盘事件。
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const auto clip = [](const char* path, int frames, float fps, bool looping = false) {
        return SpriteClip{QPixmap(path), frames, fps, looping};
    };

    m_playerArt.idle = clip(":/art/Spritesheets/Brawler Girl/idle.png", 4, 6.0f, true);
    m_playerArt.walk = clip(":/art/Spritesheets/Brawler Girl/walk.png", 10, 12.0f, true);
    m_playerArt.lightAttack = clip(":/art/Spritesheets/Brawler Girl/jab.png", 3, 16.0f);
    m_playerArt.heavyAttack = clip(":/art/Spritesheets/Brawler Girl/kick.png", 5, 18.0f);
    m_playerArt.jump = clip(":/art/Spritesheets/Brawler Girl/jump.png", 4, 7.0f);
    m_playerArt.airAttack = clip(":/art/Spritesheets/Brawler Girl/jump_kick.png", 3, 14.0f);
    m_playerArt.hurt = clip(":/art/Spritesheets/Brawler Girl/hurt.png", 2, 14.0f);
    m_playerArt.sourceFacing = Facing::Right;

    m_patrollerArt.idle = clip(":/art/Spritesheets/Enemy Punk/idle.png", 4, 6.0f, true);
    m_patrollerArt.walk = clip(":/art/Spritesheets/Enemy Punk/walk.png", 4, 8.0f, true);
    m_patrollerArt.lightAttack = clip(":/art/Spritesheets/Enemy Punk/punch.png", 3, 16.0f);
    m_patrollerArt.heavyAttack = m_patrollerArt.lightAttack;
    m_patrollerArt.hurt = clip(":/art/Spritesheets/Enemy Punk/hurt.png", 4, 14.0f);
    m_patrollerArt.dead = clip(":/art/Spritesheets/Enemy Punk/death.png", 4, 6.0f);
    m_patrollerArt.sourceFacing = Facing::Left;

    m_ambusherArt.idle = clip(":/art/伏击型飞行敌人/Idle.png", 4, 6.0f, true);
    m_ambusherArt.walk = clip(":/art/伏击型飞行敌人/Walk.png", 4, 8.0f, true);
    m_ambusherArt.ambush = clip(":/art/伏击型飞行敌人/Attack.png", 4, 8.0f);
    m_ambusherArt.hurt = clip(":/art/伏击型飞行敌人/Hurt.png", 2, 14.0f);
    m_ambusherArt.dead = clip(":/art/伏击型飞行敌人/Death.png", 4, 8.0f);
    m_ambusherArt.sourceFacing = Facing::Right;
    m_ambusherArt.horizontalPivot = 1.0f / 3.0f;

    m_chargerArt.idle = clip(":/art/冲撞型敌人/Idle.png", 4, 6.0f, true);
    m_chargerArt.walk = clip(":/art/冲撞型敌人/Walk.png", 6, 9.0f, true);
    m_chargerArt.charge = clip(":/art/冲撞型敌人/Attack.png", 4, 8.0f);
    m_chargerArt.hurt = clip(":/art/冲撞型敌人/Hurt.png", 2, 14.0f);
    m_chargerArt.dead = clip(":/art/冲撞型敌人/Death.png", 6, 8.0f);
    m_chargerArt.sourceFacing = Facing::Right;
    m_chargerArt.horizontalPivot = 1.0f / 3.0f;

    m_rangedGunnerArt.idle = clip(":/art/远程型敌人/Idle.png", 4, 6.0f, true);
    m_rangedGunnerArt.walk = clip(":/art/远程型敌人/Walk.png", 6, 9.0f, true);
    m_rangedGunnerArt.rangedAttack = clip(":/art/远程型敌人/Attack.png", 9, 28.0f);
    m_rangedGunnerArt.hurt = clip(":/art/远程型敌人/Hurt.png", 2, 14.0f);
    m_rangedGunnerArt.dead = clip(":/art/远程型敌人/Death.png", 6, 8.0f);
    m_rangedGunnerArt.sourceFacing = Facing::Right;
    m_rangedGunnerArt.horizontalPivot = 1.0f / 3.0f;

    m_rangedRobotArt.idle = clip(":/art/远程型敌人2（机器人）/Idle.png", 4, 6.0f, true);
    m_rangedRobotArt.walk = clip(":/art/远程型敌人2（机器人）/Walk.png", 4, 8.0f, true);
    m_rangedRobotArt.rangedAttack = clip(":/art/远程型敌人2（机器人）/Attack.png", 4, 14.0f);
    m_rangedRobotArt.hurt = clip(":/art/远程型敌人2（机器人）/Hurt.png", 2, 14.0f);
    m_rangedRobotArt.dead = clip(":/art/远程型敌人2（机器人）/Death.png", 4, 6.0f);
    m_rangedRobotArt.sourceFacing = Facing::Right;
    m_rangedRobotArt.horizontalPivot = 1.0f / 3.0f;

    m_bossArt.idle = clip(":/art/Boss/Idle.png", 4, 5.0f, true);
    m_bossArt.walk = clip(":/art/Boss/Walk.png", 6, 9.0f, true);
    m_bossArt.lightAttack = clip(":/art/Boss/Attack1.png", 8, 12.0f);
    m_bossArt.heavyAttack = clip(":/art/Boss/Attack2.png", 6, 10.0f);
    m_bossArt.rangedAttack = clip(":/art/Boss/Attack3.png", 4, 8.0f);
    m_bossArt.charge = m_bossArt.heavyAttack;
    m_bossArt.airAttack = clip(":/art/Boss/Attack4.png", 6, 10.0f);
    m_bossArt.ambush = clip(":/art/Boss/Sneer.png", 6, 8.0f);
    m_bossArt.hurt = clip(":/art/Boss/Hurt.png", 2, 14.0f);
    m_bossArt.dead = clip(":/art/Boss/Death.png", 6, 8.0f);
    m_bossArt.sourceFacing = Facing::Right;

    m_actorShadow = QPixmap(":/art/Sprites/shadow.png");
    m_robotProjectile = QPixmap(":/art/远程型敌人2（机器人）/Ball1.png");
    m_robotProjectileAlt = QPixmap(":/art/远程型敌人2（机器人）/Ball2.png");
    m_stageTileset = QPixmap(":/art/Stage Layers/tileset.png");
    m_stageBack = QPixmap(":/art/Stage Layers/back.png");
    m_stageFore = QPixmap(":/art/Stage Layers/fore.png");
    m_stageCar = QPixmap(":/art/Stage Layers/props/car.png");
    m_stageBarrel = QPixmap(":/art/Stage Layers/props/barrel.png");
    m_stageHydrant = QPixmap(":/art/Stage Layers/props/hydrant.png");

    for (std::size_t i = 0; i < m_healthBarArt.size(); ++i) {
        const auto suffix = QString::number(static_cast<int>(i + 1));
        m_healthBarArt[i] = QPixmap(":/art/HUD/HealthBar" + suffix + ".png");
        m_energyBarArt[i] = QPixmap(":/art/HUD/EnergyBar" + suffix + ".png");
    }
    for (std::size_t i = 0; i < m_hudScrollArt.size(); ++i) {
        const auto suffix = QString::number(static_cast<int>(i + 1));
        m_hudScrollArt[i] = QPixmap(":/art/HUD/Scrolling" + suffix + ".png");
    }

    m_interfaceFrameArt = {
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
    m_interfaceLogoArt = {
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
    m_redButtonArt = loadButton("1");
    m_cyanButtonArt = loadButton("2");
    m_greenButtonArt = loadButton("7");

    m_startIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_20.png");
    m_pauseIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_05.png");
    m_bossIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_10.png");
    m_winIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_02.png");
    m_lossIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_19.png");
    m_restartIcon = QPixmap(":/art/界面图/9 Other/3 Skill icons/Skillicon7_17.png");

    const int fontId = QFontDatabase::addApplicationFont(
        ":/art/界面图/10 Font/CyberpunkCraftpixPixel.otf");
    if (fontId >= 0) {
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) m_interfaceFontFamily = families.front();
    }

    // 游戏循环定时器 ~60 FPS
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!m_elapsed.isValid()) {
            m_elapsed.start();
        }
        const float dt = static_cast<float>(m_elapsed.restart()) / 1000.0f;
        const float clampedDt = std::min(dt, 0.1f);
        ++m_frameIndex;

        m_goBlinkTimer += clampedDt;

        // 血量延迟条：追赶实际血量
        if (m_gameState && m_gameState->player.health.maximum > 0) {
            const float targetRatio = static_cast<float>(m_gameState->player.health.current) /
                                      static_cast<float>(m_gameState->player.health.maximum);
            m_displayHealthRatio += (targetRatio - m_displayHealthRatio) * clampedDt * 4.0f;
        }

        // 屏幕震动计时
        if (m_gameState && m_gameState->player.impactRevision != m_lastPlayerImpactRev) {
            m_lastPlayerImpactRev = m_gameState->player.impactRevision;
            if (m_gameState->player.lastImpact != ImpactLevel::None) {
                m_screenShakeTimer = (m_gameState->player.lastImpact == ImpactLevel::Heavy) ? 0.15f : 0.06f;
                SoundManager::play("player_hurt");
            }
        }
        if (m_screenShakeTimer > 0.0f) {
            m_screenShakeTimer = std::max(0.0f, m_screenShakeTimer - clampedDt);
        }

        // 拾取检测：pickup 数量减少即被捡走
        if (m_gameState) {
            const auto curCount = m_gameState->pickups.size();
            if (curCount < m_lastPickupCount && !m_gameState->pickups.empty()) {
                // 有新拾取发生但列表非空，记录拾取效果
            }
            if (curCount < m_lastPickupCount) {
                m_pickupFxTimer = 0.5f;
            }
            m_lastPickupCount = curCount;
        }
        if (m_pickupFxTimer > 0.0f) {
            m_pickupFxTimer = std::max(0.0f, m_pickupFxTimer - clampedDt);
        }

        // 敌人攻击动画检测：玩家扣血时找最近的敌人
        if (m_gameState && m_gameState->player.health.current < m_prevPlayerHp) {
            float minDist = 9999.0f;
            std::uint32_t closestId = 0;
            for (const auto& e : m_gameState->enemies) {
                if (!e.visible || e.actionState == ActorActionState::Dead) continue;
                float dx = e.position.x - m_gameState->player.position.x;
                float dy = e.position.laneY - m_gameState->player.position.laneY;
                float dist = dx * dx + dy * dy;
                if (dist < minDist) { minDist = dist; closestId = e.id; }
            }
            if (closestId != 0) {
                m_attackingEnemyId = closestId;
                m_attackingEnemyTimer = 0.25f;
            }
        }
        if (m_attackingEnemyTimer > 0.0f) {
            m_attackingEnemyTimer = std::max(0.0f, m_attackingEnemyTimer - clampedDt);
        }
        if (m_gameState) m_prevPlayerHp = m_gameState->player.health.current;

        if (m_tickCommand) m_tickCommand(clampedDt, m_frameIndex);

        if (m_gameState) {
            const auto phase = m_gameState->phase;
            if (phase != m_observedGamePhase) {
                if (phase == GamePhase::GameOver && !m_gameOverSounded) {
                    SoundManager::play("gameover");
                    m_gameOverSounded = true;
                } else if (phase == GamePhase::Win && !m_winSounded) {
                    SoundManager::play("win");
                    m_winSounded = true;
                }
            }
            const bool startingRound = phase == GamePhase::Playing &&
                                       (m_observedGamePhase == GamePhase::Title ||
                                        m_observedGamePhase == GamePhase::GameOver ||
                                        m_observedGamePhase == GamePhase::Win);
            if (startingRound) {
                m_animationClocks.clear();
                m_gameOverSounded = false;
                m_winSounded = false;
                m_attackingEnemyId = 0;
                m_attackingEnemyTimer = 0.0f;
                m_prevPlayerHp = 100;
                m_lastPickupCount = 0;
                m_pickupFxTimer = 0.0f;
                m_displayHealthRatio = 1.0f;
                m_lastPlayerImpactRev = 0;
            }
            m_observedGamePhase = phase;

            const auto& encounter = m_gameState->encounter;
            const bool enteringBossIntro = encounter.kind == EncounterKind::Boss &&
                                           encounter.phase == EncounterPhase::Intro &&
                                           (m_observedEncounterKind != EncounterKind::Boss ||
                                            m_observedEncounterPhase != EncounterPhase::Intro);
            if (enteringBossIntro) SoundManager::play("boss_intro");
            m_observedEncounterKind = encounter.kind;
            m_observedEncounterPhase = encounter.phase;

            updateActorAnimation(m_gameState->player, clampedDt);
            for (const auto& enemy : m_gameState->enemies) {
                updateActorAnimation(enemy, clampedDt);
            }
        }
    });

    // 启动定时器
    m_elapsed.start();
    m_timer.start(16); // ~60 FPS

    // 音效初始化
    SoundManager::init("assets/sfx/");
}

void GameWidget::set_game_state(const GameState* state) noexcept
{
    const bool firstBinding = m_gameState == nullptr;
    m_gameState = state;
    if (firstBinding && state != nullptr) {
        m_observedGamePhase = state->phase;
        m_observedEncounterKind = state->encounter.kind;
        m_observedEncounterPhase = state->encounter.phase;
    }
    update();
}

const GameState& GameWidget::game_state() const noexcept
{
    return m_gameState != nullptr ? *m_gameState : m_emptyState;
}

void GameWidget::setRunning(bool running)
{
    if (running) {
        if (!m_timer.isActive()) {
            m_elapsed.start();
            m_timer.start(16);
        }
    } else {
        m_timer.stop();
    }
}

// ============================================================================
// 坐标转换
// ============================================================================

float GameWidget::worldToScreenX(float worldX) const
{
    return (worldX - game_state().map.cameraX) * m_scaleX;
}

float GameWidget::worldToScreenY(float laneY, float z) const
{
    // laneY 是街道纵深，z 是离地高度（跳跃时 > 0）
    return (laneY - z) * m_scaleY;
}

// ============================================================================
// 键盘输入处理
//
// 按键映射全部集中在这里，换手柄 / 触屏只需改这两个函数。
// MovementIntent 聚合移动键状态，具体命令由 App 从 ViewModel 注入。
// ============================================================================

bool GameWidget::isHandledKey(int qtKey)
{
    switch (qtKey) {
    case Qt::Key_Left:  case Qt::Key_A:
    case Qt::Key_Right: case Qt::Key_D:
    case Qt::Key_Up:    case Qt::Key_W:
    case Qt::Key_Down:  case Qt::Key_S:
    case Qt::Key_J:     case Qt::Key_Z:
    case Qt::Key_K:     case Qt::Key_X:
    case Qt::Key_Space:
    case Qt::Key_R:
    case Qt::Key_Return: case Qt::Key_Enter:
    case Qt::Key_Escape: case Qt::Key_P:
        return true;
    default:
        return false;
    }
}

void GameWidget::emitMovement()
{
    // 预留给后续把四方向聚合成一个移动命令。
}

void GameWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    const int key = event->key();
    if (!isHandledKey(key)) {
        QWidget::keyPressEvent(event);
        return;
    }

    // ---- 移动键：更新 MovementIntent 并维持 Pressed 状态 ----
    switch (key) {
    case Qt::Key_Left:  case Qt::Key_A:
        if (!m_movement.left) {
            m_movement.left = true;
            if (m_moveLeftCommand) m_moveLeftCommand(true);
        }
        return;
    case Qt::Key_Right: case Qt::Key_D:
        if (!m_movement.right) {
            m_movement.right = true;
            if (m_moveRightCommand) m_moveRightCommand(true);
        }
        return;
    case Qt::Key_Up:    case Qt::Key_W:
        if (!m_movement.up) {
            m_movement.up = true;
            if (m_moveUpCommand) m_moveUpCommand(true);
        }
        return;
    case Qt::Key_Down:  case Qt::Key_S:
        if (!m_movement.down) {
            m_movement.down = true;
            if (m_moveDownCommand) m_moveDownCommand(true);
        }
        return;
    default: break;
    }

    // ---- 单次触发动作 ----
    if (m_triggeredThisPress.contains(key)) return;
    m_triggeredThisPress.insert(key);

    switch (key) {
    case Qt::Key_J: case Qt::Key_Z:
        if (m_primaryActionCommand) { m_primaryActionCommand(); SoundManager::play("hit_light"); }
        break;
    case Qt::Key_K: case Qt::Key_X:
        if (m_secondaryActionCommand) { m_secondaryActionCommand(); SoundManager::play("hit_heavy"); }
        break;
    case Qt::Key_Space:
        if (m_stateToggleCommand) { m_stateToggleCommand(); SoundManager::play("jump"); }
        break;
    case Qt::Key_R:
        if (m_resetCommand) m_resetCommand();
        break;
    case Qt::Key_Return: case Qt::Key_Enter:
        if (m_confirmCommand) m_confirmCommand();
        break;
    case Qt::Key_Escape: case Qt::Key_P:
        if (m_pauseCommand) m_pauseCommand();
        break;
    default: break;
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    const int key = event->key();
    if (!isHandledKey(key)) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    m_triggeredThisPress.remove(key);

    // ---- 移动键释放：更新 MovementIntent ----
    switch (key) {
    case Qt::Key_Left:  case Qt::Key_A:
        m_movement.left = false;
        if (m_moveLeftCommand) m_moveLeftCommand(false);
        break;
    case Qt::Key_Right: case Qt::Key_D:
        m_movement.right = false;
        if (m_moveRightCommand) m_moveRightCommand(false);
        break;
    case Qt::Key_Up:    case Qt::Key_W:
        m_movement.up = false;
        if (m_moveUpCommand) m_moveUpCommand(false);
        break;
    case Qt::Key_Down:  case Qt::Key_S:
        m_movement.down = false;
        if (m_moveDownCommand) m_moveDownCommand(false);
        break;
    default: break;
    }
}

// ============================================================================
// 窗口大小变化
// ============================================================================

void GameWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;

    m_scaleX = static_cast<float>(width()) / vpW;
    m_scaleY = static_cast<float>(height()) / vpH;
}

// ============================================================================
// 主绘制入口
// ============================================================================

void GameWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 黑色背景
    p.fillRect(rect(), QColor(0, 0, 0));

    // 更新缩放
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    m_scaleX = static_cast<float>(width()) / vpW;
    m_scaleY = static_cast<float>(height()) / vpH;

    const GamePhase phase = game_state().phase;

    // ---- 标题画面 ----
    if (phase == GamePhase::Title) {
        drawBackground(p);
        drawStreet(p);
        drawForeground(p);
        drawOverlay(p);
        return;
    }

    // ---- 屏幕震动偏移 ----
    p.save();
    float shakeX = 0.0f, shakeY = 0.0f;
    if (m_screenShakeTimer > 0.0f) {
        const float intensity = m_screenShakeTimer * 20.0f * m_scaleX;
        shakeX = std::sin(m_goBlinkTimer * 60.0f) * intensity;
        shakeY = std::cos(m_goBlinkTimer * 53.0f) * intensity * 0.5f;
        p.translate(shakeX, shakeY);
    }

    // ---- 游戏画面 ----
    drawBackground(p);
    drawStreet(p);

    // 按 depthSortY 排序绘制所有角色（远的先画）
    std::vector<const ActorState*> drawList;

    // 玩家
    if (game_state().player.visible) {
        drawList.push_back(&game_state().player);
    }

    // 敌人
    for (const auto& enemy : game_state().enemies) {
        if (enemy.visible) {
            drawList.push_back(&enemy);
        }
    }

    // 按 laneY + z 排序，远的先画。
    std::sort(drawList.begin(), drawList.end(),
              [](const ActorState* a, const ActorState* b) {
                  const float ya = a->position.laneY + a->position.z;
                  const float yb = b->position.laneY + b->position.z;
                  return ya < yb;
              });

    for (const auto* actor : drawList) {
        drawActor(p, *actor);
    }

    drawForeground(p);

    // ---- 飞行道具 ----
    for (const auto& proj : game_state().projectiles) {
        drawProjectile(p, proj);
    }

    // ---- 掉落物 ----
    for (const auto& pkp : game_state().pickups) {
        drawPickup(p, pkp);
    }

    // ---- 粒子特效 ----
    drawParticles(p);

    // ---- 拾取特效：飘字 ----
    if (m_pickupFxTimer > 0.0f) {
        const float px = worldToScreenX(game_state().player.position.x);
        const float py = worldToScreenY(game_state().player.position.laneY, game_state().player.position.z) - 60.0f * m_scaleY;
        const float alpha = m_pickupFxTimer / 0.5f;
        const float rise = (0.5f - m_pickupFxTimer) * 30.0f * m_scaleY;
        p.setPen(QColor(100, 255, 100, static_cast<int>(255 * alpha)));
        p.setFont(QFont("Arial", 14, QFont::Bold));
        p.drawText(QRectF(px - 30.0f * m_scaleX, py + rise - 15.0f * m_scaleY,
                          60.0f * m_scaleX, 30.0f * m_scaleY),
                   Qt::AlignCenter, "+");
    }
    p.restore();

    // ---- HUD 与遭遇页面不跟随世界震动 ----
    drawHUD(p);
    drawEncounterOverlay(p);

    // ---- 覆盖层（暂停 / 结算 / 胜利） ----
    if (phase == GamePhase::Paused   ||
        phase == GamePhase::GameOver ||
        phase == GamePhase::Win) {
        drawOverlay(p);
    }
}

// ============================================================================
// 背景绘制
// ============================================================================

void GameWidget::drawBackground(QPainter& p)
{
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    const float streetTop = game_state().map.streetTopY > 0.0f
                                ? game_state().map.streetTopY : 300.0f;

    if (!m_stageTileset.isNull()) {
        p.fillRect(QRectF(0, 0, vpW * m_scaleX, vpH * m_scaleY),
                   QColor(8, 11, 16));

        if (!m_stageBack.isNull()) {
            p.save();
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            const float tileW = 192.0f * m_scaleX;
            const float tileH = 160.0f * m_scaleY;
            const float parallaxOffset = std::fmod(game_state().map.cameraX * 0.15f * m_scaleX,
                                                   tileW);
            for (float x = -parallaxOffset - tileW; x < width() + tileW; x += tileW) {
                p.drawPixmap(QRectF(x, 8.0f * m_scaleY, tileW, tileH),
                             m_stageBack, QRectF(m_stageBack.rect()));
            }
            p.restore();
        }

        drawBuildings(p);
        return;
    }

    // 天空渐变
    QLinearGradient skyGrad(0, 0, 0, streetTop * m_scaleY);
    skyGrad.setColorAt(0.0, QColor(30, 40, 80));
    skyGrad.setColorAt(0.6, QColor(60, 80, 140));
    skyGrad.setColorAt(1.0, QColor(120, 150, 200));
    p.fillRect(QRectF(0, 0, vpW * m_scaleX, streetTop * m_scaleY), skyGrad);

    // 星星与月亮
    if (!m_starsGenerated) {
        m_stars.clear();
        for (int i = 0; i < 35; ++i) {
            m_stars.push_back(QPointF(
                static_cast<float>(std::rand() % static_cast<int>(vpW)),
                static_cast<float>(std::rand() % static_cast<int>(streetTop * 0.7f))));
        }
        m_starsGenerated = true;
    }
    p.setPen(Qt::NoPen);
    for (const auto& star : m_stars) {
        const float twinkle = 0.5f + 0.5f * std::sin(m_goBlinkTimer * 3.0f + star.x() * 0.1f);
        p.setBrush(QColor(255, 255, 240, static_cast<int>(120 * twinkle + 60)));
        p.drawEllipse(star.x() * m_scaleX, star.y() * m_scaleY, 2.0f * m_scaleX, 2.0f * m_scaleY);
    }
    // 月亮
    p.setBrush(QColor(255, 250, 210, 180));
    const float moonX = 720.0f * m_scaleX;
    const float moonY = 55.0f * m_scaleY;
    const float moonR = 28.0f * m_scaleX;
    p.drawEllipse(QPointF(moonX, moonY), moonR, moonR);
    p.setBrush(QColor(30, 40, 80));
    p.drawEllipse(QPointF(moonX + moonR * 0.35f, moonY - moonR * 0.1f), moonR * 0.85f, moonR * 0.85f);

    // 远景建筑
    drawBuildings(p);
}

void GameWidget::drawBuildings(QPainter& p)
{
    const float streetTop = game_state().map.streetTopY > 0.0f
                                ? game_state().map.streetTopY : 300.0f;
    const float camX = game_state().map.cameraX;
    const float parallaxFactor = 0.3f;

    if (!m_stageTileset.isNull()) {
        const QRect shopfronts[] = {
            QRect(0, 0, 224, 96),
            QRect(288, 0, 224, 96),
        };

        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.fillRect(QRectF(0, 55.0f * m_scaleY,
                          width(), 165.0f * m_scaleY),
                   QColor(18, 20, 23));

        float worldX = 0.0f;
        int segmentIndex = 0;
        while (worldX < 3400.0f) {
            const QRect source = shopfronts[segmentIndex % 2];
            const float worldW = source.width() * 2.0f;
            const float screenX = (worldX - camX) * m_scaleX;
            const QRectF target(screenX, 28.0f * m_scaleY,
                                worldW * m_scaleX, 192.0f * m_scaleY);
            if (target.right() >= 0.0f && target.left() <= width()) {
                p.drawPixmap(target, m_stageTileset, QRectF(source));
            }
            worldX += worldW;
            ++segmentIndex;
        }

        p.restore();
        return;
    }

    struct Building {
        float x, width, height;
        QColor body, windowLit, windowDark, roof;
    };

    const Building buildings[] = {
        { 100, 120, 180, QColor(45,42,60),   QColor(255,235,170), QColor(35,30,50),   QColor(35,32,48) },
        { 280,  90, 140, QColor(55,48,62),   QColor(255,220,130), QColor(40,35,50),   QColor(45,38,52) },
        { 450, 150, 210, QColor(42,40,58),   QColor(255,240,180), QColor(30,28,45),   QColor(32,30,48) },
        { 680, 105, 160, QColor(50,44,60),   QColor(255,230,160), QColor(38,32,48),   QColor(40,34,50) },
        { 850, 130, 190, QColor(48,43,65),   QColor(255,225,150), QColor(35,30,52),   QColor(38,33,52) },
        { 1050,110, 155, QColor(52,46,58),   QColor(255,235,165), QColor(40,35,46),   QColor(42,36,48) },
        { 1250,140, 215, QColor(40,38,56),   QColor(255,240,175), QColor(28,26,44),   QColor(30,27,45) },
        { 1480,100, 170, QColor(47,41,59),   QColor(255,230,155), QColor(35,30,48),   QColor(37,32,49) },
        { 1680,155, 200, QColor(44,40,62),   QColor(255,220,140), QColor(32,28,50),   QColor(34,30,52) },
        { 1900,120, 160, QColor(51,45,57),   QColor(255,235,170), QColor(39,34,45),   QColor(41,36,47) },
        { 2100,135, 190, QColor(43,39,63),   QColor(255,225,148), QColor(31,27,50),   QColor(33,29,53) },
        { 2350,145, 210, QColor(46,41,61),   QColor(255,240,185), QColor(34,29,49),   QColor(36,31,51) },
        { 2600,115, 168, QColor(49,43,56),   QColor(255,230,160), QColor(37,32,44),   QColor(39,34,46) },
        { 2800,125, 178, QColor(44,40,60),   QColor(255,220,145), QColor(32,28,48),   QColor(34,30,50) },
    };

    for (const auto& b : buildings) {
        const float sx = (b.x - camX * parallaxFactor) * m_scaleX;
        const float sw = b.width * m_scaleX;
        const float bottomY = streetTop * m_scaleY;
        const float sh = b.height * m_scaleY;
        const float sy = bottomY - sh;
        if (sx + sw < 0 || sx > width()) continue;

        // 建筑主体
        p.fillRect(QRectF(sx, sy, sw, sh), b.body);

        // 楼顶装饰线
        p.fillRect(QRectF(sx, sy, sw, 4.0f * m_scaleY), b.roof.lighter(140));

        // 大窗户（2列 x 3行）
        const float winW = 14.0f * m_scaleX;
        const float winH = 16.0f * m_scaleY;
        const float marginX = 10.0f * m_scaleX;
        const float gapX = (sw - marginX * 2 - winW * 2) / 1.0f;
        const float startY = sy + 14.0f * m_scaleY;
        const float gapY = 22.0f * m_scaleY;

        for (int col = 0; col < 2; ++col) {
            const float wx = sx + marginX + col * (winW + gapX);
            for (int row = 0; row < 3; ++row) {
                const float wy = startY + row * gapY;
                if (wy + winH > bottomY - 4.0f * m_scaleY) break;
                const bool lit = (static_cast<int>(wx * 137 + wy * 251 + b.x * 73) % 4) > 0;
                p.fillRect(QRectF(wx, wy, winW, winH), lit ? b.windowLit : b.windowDark);
                p.setPen(QPen(b.roof, 0.8f));
                p.drawRect(QRectF(wx, wy, winW, winH));
            }
        }

        // 地面入口门
        const float doorW = 12.0f * m_scaleX;
        const float doorH = 20.0f * m_scaleY;
        const float doorX = sx + sw * 0.5f - doorW * 0.5f;
        const float doorY = bottomY - doorH - 2.0f * m_scaleY;
        p.fillRect(QRectF(doorX, doorY, doorW, doorH), QColor(25, 20, 30));
        p.setPen(QPen(b.roof, 1.0f));
        p.drawRect(QRectF(doorX, doorY, doorW, doorH));

        // 楼顶边缘
        p.setPen(Qt::NoPen);
        p.fillRect(QRectF(sx - 1.0f, sy - 2.0f * m_scaleY, sw + 2.0f, 3.0f * m_scaleY), b.roof);
    }
}

void GameWidget::drawStreet(QPainter& p)
{
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    const float streetTop = game_state().map.streetTopY > 0.0f
                                ? game_state().map.streetTopY : 300.0f;
    const float streetBottom = game_state().map.streetBottomY > 0.0f
                                   ? game_state().map.streetBottomY : 500.0f;
    const float camX = game_state().map.cameraX;

    if (!m_stageTileset.isNull()) {
        const QRect streetSource(48, 224, 112, 144);
        constexpr float tileWorldHeight = 320.0f;
        constexpr float tileWorldWidth = 112.0f * tileWorldHeight / 144.0f;
        constexpr float streetWorldY = 220.0f;

        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.fillRect(QRectF(0, streetWorldY * m_scaleY,
                          vpW * m_scaleX, (vpH - streetWorldY) * m_scaleY),
                   QColor(24, 42, 50));

        const float firstTileX = std::floor(camX / tileWorldWidth) * tileWorldWidth
                               - tileWorldWidth;
        for (float worldX = firstTileX;
             worldX < camX + vpW + tileWorldWidth;
             worldX += tileWorldWidth) {
            const float screenX = (worldX - camX) * m_scaleX;
            p.drawPixmap(QRectF(screenX, streetWorldY * m_scaleY,
                                tileWorldWidth * m_scaleX,
                                tileWorldHeight * m_scaleY),
                         m_stageTileset, QRectF(streetSource));
        }

        const auto drawProp = [&](const QPixmap& pixmap, float worldX,
                                  float bottomY, float artScale) {
            if (pixmap.isNull()) return;
            const float targetW = pixmap.width() * artScale;
            const float targetH = pixmap.height() * artScale;
            const float screenX = (worldX - camX) * m_scaleX;
            const QRectF target(screenX, (bottomY - targetH) * m_scaleY,
                                targetW * m_scaleX, targetH * m_scaleY);
            if (target.right() >= 0.0f && target.left() <= width()) {
                p.drawPixmap(target, pixmap, QRectF(pixmap.rect()));
            }
        };

        drawProp(m_stageHydrant, 250.0f, 330.0f, 2.0f);
        drawProp(m_stageBarrel, 560.0f, 340.0f, 2.0f);
        drawProp(m_stageCar, 780.0f, 305.0f, 2.0f);
        drawProp(m_stageHydrant, 1320.0f, 330.0f, 2.0f);
        drawProp(m_stageBarrel, 1650.0f, 340.0f, 2.0f);
        drawProp(m_stageCar, 2150.0f, 305.0f, 2.0f);
        drawProp(m_stageHydrant, 2470.0f, 330.0f, 2.0f);
        drawProp(m_stageBarrel, 2760.0f, 340.0f, 2.0f);

        p.restore();
        return;
    }

    const float sy0 = streetTop * m_scaleY;
    const float sy1 = streetBottom * m_scaleY;
    const float streetH = sy1 - sy0;

    // 人行道
    p.fillRect(QRectF(0, sy0, vpW * m_scaleX, streetH * 0.15f),
               QColor(140, 130, 120));

    // 车道
    p.fillRect(QRectF(0, sy0 + streetH * 0.15f, vpW * m_scaleX, streetH * 0.70f),
               QColor(70, 65, 60));

    // 人行道（下方）
    p.fillRect(QRectF(0, sy0 + streetH * 0.85f, vpW * m_scaleX, streetH * 0.15f),
               QColor(140, 130, 120));

    // 车道虚线（跟随镜头滚动）
    p.setPen(QPen(QColor(180, 175, 70), 2.0f * m_scaleY, Qt::DashLine));
    const float dashY = sy0 + streetH * 0.5f;
    const float dashOffset = std::fmod(camX * m_scaleX, 40.0f * m_scaleX);
    // 用实线画虚线效果
    p.setPen(Qt::NoPen);
    const float dashLen = 25.0f * m_scaleX;
    const float gapLen = 15.0f * m_scaleX;
    const float dashH = 3.0f * m_scaleY;
    for (float dx = -dashOffset; dx < vpW * m_scaleX; dx += dashLen + gapLen) {
        p.fillRect(QRectF(dx, dashY - dashH / 2, dashLen, dashH),
                   QColor(200, 195, 100));
    }

    // 路缘石
    p.fillRect(QRectF(0, sy0 + streetH * 0.15f - 2.0f * m_scaleY,
                      vpW * m_scaleX, 3.0f * m_scaleY),
               QColor(160, 150, 140));
    p.fillRect(QRectF(0, sy0 + streetH * 0.85f,
                      vpW * m_scaleX, 3.0f * m_scaleY),
               QColor(160, 150, 140));

    // 地面下方（街沿以下）
    p.fillRect(QRectF(0, sy1, vpW * m_scaleX, vpH * m_scaleY - sy1),
               QColor(40, 38, 35));
}

void GameWidget::drawForeground(QPainter& p)
{
    if (m_stageFore.isNull()) return;

    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    const float camX = game_state().map.cameraX;
    const float targetW = m_stageFore.width() * 2.0f;
    const float targetH = m_stageFore.height() * 2.0f;
    const float positions[] = {620.0f, 1800.0f, 2920.0f};

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const float worldX : positions) {
        const float screenX = (worldX - camX) * m_scaleX;
        const QRectF target(screenX, (vpH - targetH) * m_scaleY,
                            targetW * m_scaleX, targetH * m_scaleY);
        if (target.right() >= 0.0f && target.left() <= width()) {
            p.drawPixmap(target, m_stageFore, QRectF(m_stageFore.rect()));
        }
    }
    p.restore();
}

// ============================================================================
// 角色绘制
// ============================================================================

QColor GameWidget::actorBodyColor(Team team, ActorKind kind)
{
    switch (team) {
    case Team::Player:
        return QColor(50, 100, 220);
    case Team::Enemy:
        switch (kind) {
        case ActorKind::Boss:     return QColor(180, 40, 40);
        case ActorKind::Patroller: return QColor(200, 70, 50);
        case ActorKind::Ambusher:  return QColor(160, 60, 140);
        case ActorKind::Charger:   return QColor(200, 130, 30);
        case ActorKind::Ranged:    return QColor(60, 150, 80);
        default:                   return QColor(200, 70, 50);
        }
    default:
        return QColor(150, 150, 150);
    }
}

QColor GameWidget::healthBarColor(float ratio)
{
    if (ratio > 0.5f) return QColor(60, 200, 60);   // 绿
    if (ratio > 0.25f) return QColor(220, 180, 30);  // 黄
    return QColor(220, 40, 40);                       // 红
}

QColor GameWidget::energyBarColor(float ratio)
{
    Q_UNUSED(ratio);
    return QColor(60, 140, 240); // 蓝色
}

std::size_t GameWidget::hudFrameIndex(float elapsed)
{
    const auto frame = static_cast<std::size_t>(
        std::floor(std::max(0.0f, elapsed) * 8.0f));
    return frame % 4u;
}

void GameWidget::drawBar(QPainter& p, float x, float y, float w, float h,
                         float ratio, QColor fillColor, const QString& label)
{
    // 背景
    p.fillRect(QRectF(x, y, w, h), QColor(30, 30, 30, 200));
    // 填充
    const float fillW = w * std::max(0.0f, std::min(1.0f, ratio));
    p.fillRect(QRectF(x, y, fillW, h), fillColor);
    // 边框
    p.setPen(QPen(QColor(200, 200, 200), 1.0f));
    p.drawRect(QRectF(x, y, w, h));

    // 标签
    if (!label.isEmpty()) {
        p.setPen(QColor(255, 255, 255));
        QFont font("Arial", 9);
        p.setFont(font);
        p.drawText(QRectF(x, y, w, h), Qt::AlignCenter, label);
    }
}

bool GameWidget::hasActorArt(const ActorState& actor) const
{
    const auto* clip = actorClip(actor);
    return clip != nullptr && !clip->sheet.isNull() && clip->frameCount > 0;
}

const GameWidget::ActorArtSet* GameWidget::actorArtSet(const ActorState& actor) const
{
    if (actor.team == Team::Player) return &m_playerArt;

    switch (actor.kind) {
    case ActorKind::Patroller:
        return &m_patrollerArt;
    case ActorKind::Ambusher:
        return &m_ambusherArt;
    case ActorKind::Charger:
        return &m_chargerArt;
    case ActorKind::Ranged:
        return actor.visualVariant == ActorVisualVariant::RangedRobot
                   ? &m_rangedRobotArt
                   : &m_rangedGunnerArt;
    case ActorKind::Boss:
        return &m_bossArt;
    case ActorKind::Player:
        return &m_playerArt;
    }
    return nullptr;
}

const GameWidget::SpriteClip* GameWidget::actorClip(const ActorState& actor) const
{
    const auto* art = actorArtSet(actor);
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

float GameWidget::actorSpriteWidth(const ActorState& actor) const
{
    const auto* clip = actorClip(actor);
    if (clip == nullptr || clip->sheet.isNull() || clip->frameCount <= 0 ||
        clip->sheet.height() <= 0) {
        return actor.drawSize.width * m_scaleX;
    }

    const float frameWidth = static_cast<float>(clip->sheet.width()) /
                             static_cast<float>(clip->frameCount);
    return actor.drawSize.height * frameWidth /
           static_cast<float>(clip->sheet.height()) * m_scaleX;
}

void GameWidget::updateActorAnimation(const ActorState& actor, float dt)
{
    if (!hasActorArt(actor)) return;

    auto& clock = m_animationClocks[actor.id];
    if (!clock.initialized || clock.state != actor.actionState ||
        clock.impactRevision != actor.impactRevision) {
        clock.state = actor.actionState;
        clock.impactRevision = actor.impactRevision;
        clock.elapsed = 0.0f;
        clock.initialized = true;
        return;
    }

    clock.elapsed += dt;
}

float GameWidget::actorAnimationElapsed(const ActorState& actor) const
{
    const auto it = m_animationClocks.find(actor.id);
    return it != m_animationClocks.end()
               ? it->second.elapsed
               : game_state().elapsedSeconds;
}

bool GameWidget::drawActorSprite(QPainter& p, const ActorState& actor)
{
    const auto* art = actorArtSet(actor);
    const auto* clip = actorClip(actor);
    if (art == nullptr || clip == nullptr || clip->sheet.isNull() ||
        clip->frameCount <= 0) {
        return false;
    }

    const float elapsed = actorAnimationElapsed(actor);
    int frameIndex = static_cast<int>(elapsed * clip->framesPerSecond);
    frameIndex = clip->looping ? frameIndex % clip->frameCount
                               : std::min(frameIndex, clip->frameCount - 1);
    frameIndex = std::max(frameIndex, 0);

    const int frameWidth = clip->sheet.width() / clip->frameCount;
    const int frameHeight = clip->sheet.height();
    const QRect source(frameIndex * frameWidth, 0, frameWidth, frameHeight);
    const float targetHeight = actor.drawSize.height * m_scaleY;
    const float targetWidth = actor.drawSize.height *
                              static_cast<float>(frameWidth) /
                              static_cast<float>(frameHeight) * m_scaleX;

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const bool flipHorizontally = actor.facing != art->sourceFacing;
    if (flipHorizontally) {
        p.scale(-1.0f, 1.0f);
    }
    if (actor.actionState == ActorActionState::Hurt) {
        const float blink = 0.62f + 0.38f * std::abs(std::sin(m_goBlinkTimer * 24.0f));
        p.setOpacity(blink);
    }
    p.drawPixmap(QRectF(-targetWidth * art->horizontalPivot, 0.0f,
                        targetWidth, targetHeight),
                 clip->sheet, QRectF(source));
    p.restore();
    return true;
}

void GameWidget::drawActor(QPainter& p, const ActorState& actor)
{
    const float screenX = worldToScreenX(actor.position.x);
    const float screenY = worldToScreenY(actor.position.laneY, actor.position.z);

    const float w = actor.drawSize.width * m_scaleX;
    const float h = actor.drawSize.height * m_scaleY;

    // 绘制矩形范围的中心
    const float cx = screenX;
    const float cy = screenY - h; // 脚底 = laneY，头顶 = laneY - drawSize.height

    const bool useActorArt = hasActorArt(actor);
    const float cullWidth = useActorArt ? actorSpriteWidth(actor) : w;

    // 裁剪：不在屏幕内的跳过
    if (cx + cullWidth / 2 < -50 || cx - cullWidth / 2 > width() + 50 ||
        cy + h < -50 || cy > height() + 50) {
        return;
    }

    const QColor bodyColor = actorBodyColor(actor.team, actor.kind);

    if (useActorArt && !m_actorShadow.isNull()) {
        const float groundY = worldToScreenY(actor.position.laneY, 0.0f);
        const float shadowWorldW = actor.drawSize.width * 0.85f;
        const float shadowWorldH = shadowWorldW * 0.16f;
        const float shadowOpacity = std::clamp(0.68f - actor.position.z / 220.0f,
                                               0.22f, 0.68f);
        p.save();
        p.setOpacity(shadowOpacity);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawPixmap(QRectF(screenX - shadowWorldW * m_scaleX * 0.5f,
                            groundY - shadowWorldH * m_scaleY * 0.5f,
                            shadowWorldW * m_scaleX,
                            shadowWorldH * m_scaleY),
                     m_actorShadow, QRectF(m_actorShadow.rect()));
        p.restore();
    }

    p.save();
    p.translate(cx, cy);

    if (!drawActorSprite(p, actor)) {
        if (actor.facing == Facing::Left) {
            p.scale(-1.0f, 1.0f);
        }
        if (actor.actionState == ActorActionState::Hurt) {
            p.fillRect(QRectF(-w * 0.5f, 0, w, h), QColor(255, 255, 255, 140));
        }
        drawCharacterBody(p, actor, bodyColor);
    }

    p.restore();

    // 血条（敌人和 Boss 头顶）
    if (actor.team == Team::Enemy && actor.health.maximum > 0 &&
        actor.actionState != ActorActionState::Dead) {
        drawHealthBar(p, actor);
    }

}

void GameWidget::drawCharacterBody(QPainter& p, const ActorState& actor,
                                    QColor bodyColor)
{
    const float w = actor.drawSize.width * m_scaleX;
    const float h = actor.drawSize.height * m_scaleY;

    // 各部位颜色
    const QColor skinColor(255, 210, 170);
    const QColor pantsColor(30, 30, 80);
    const QColor shoeColor(50, 40, 35);
    const QColor headColor = skinColor;
    const QColor darkerBody = bodyColor.darker(130);

    // 身体比例
    const float headR = w * 0.22f;
    const float bodyW = w * 0.55f;
    const float bodyH = h * 0.35f;
    const float bodyTop = h * 0.18f;
    const float legW = w * 0.18f;
    const float legH = h * 0.35f;
    const float legTop = bodyTop + bodyH;
    const float armW = w * 0.15f;
    const float armH = h * 0.28f;

    const ActorActionState actionState = actor.actionState;

    // ---- Idle 呼吸微动 ----
    if (actionState == ActorActionState::Idle) {
        const float breathe = std::sin(game_state().elapsedSeconds * 3.0f) * 2.0f * m_scaleY;
        p.translate(0, breathe);
    }

    // ---- 阴影（地面上椭圆） ----
    p.setPen(Qt::NoPen);
    p.fillRect(QRectF(-w * 0.35f, h - 4.0f * m_scaleY, w * 0.7f, 4.0f * m_scaleY),
               QColor(0, 0, 0, 60));

    // ---- 行走身体上下摆动 ----
    if (actionState == ActorActionState::Walk) {
        const float bob = std::abs(std::sin(game_state().elapsedSeconds * 10.0f)) * 3.0f * m_scaleY;
        p.translate(0, -bob);
    }

    // ---- 腿 ----
    const float legGap = legW * 0.2f;  // 两腿间距
    if (actionState == ActorActionState::Jump || actionState == ActorActionState::AirAttack) {
        p.fillRect(QRectF(-legW - legGap * 0.5f, legTop, legW, legH * 0.5f), pantsColor);
        p.fillRect(QRectF(legGap * 0.5f, legTop, legW, legH * 0.5f), pantsColor);
    } else if (actionState == ActorActionState::Walk) {
        const float t = game_state().elapsedSeconds * 10.0f;
        const float stride = std::sin(t) * 6.0f * m_scaleX;
        const float kneeBend = std::abs(std::cos(t)) * 3.0f * m_scaleY;
        // 两腿交替：一前一后
        p.fillRect(QRectF(-legW * 0.35f + stride, legTop + kneeBend, legW, legH - kneeBend), pantsColor);
        p.fillRect(QRectF(-legW * 0.35f - stride, legTop - kneeBend * 0.5f, legW, legH + kneeBend * 0.5f), pantsColor);
        // 鞋
        p.fillRect(QRectF(-legW * 0.35f + stride - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
        p.fillRect(QRectF(-legW * 0.35f - stride - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
    } else {
        p.fillRect(QRectF(-legW - legGap * 0.5f, legTop, legW, legH), pantsColor);
        p.fillRect(QRectF(legGap * 0.5f, legTop, legW, legH), pantsColor);
        p.fillRect(QRectF(-legW - legGap * 0.5f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
        p.fillRect(QRectF(legGap * 0.5f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
    }

    // ---- 身体 ----
    p.fillRect(QRectF(-bodyW / 2, bodyTop, bodyW, bodyH), bodyColor);

    // 衣服纹理线
    p.setPen(QPen(darkerBody, 1.0f));
    p.drawLine(QPointF(-bodyW * 0.3f, bodyTop + bodyH * 0.3f),
               QPointF(bodyW * 0.3f, bodyTop + bodyH * 0.3f));

    // ---- 手臂 ----
    const bool isEnemyAttacking = (actor.team == Team::Enemy &&
        actor.id == m_attackingEnemyId && m_attackingEnemyTimer > 0.0f);

    if (isEnemyAttacking) {
        // 敌人攻击姿态：单手前伸
        const float extend = armW * 2.2f;
        p.fillRect(QRectF(bodyW * 0.1f, bodyTop + bodyH * 0.1f, extend, armH * 0.45f), skinColor);
        p.fillRect(QRectF(bodyW * 0.1f + extend - 4.0f * m_scaleX,
                          bodyTop + bodyH * 0.1f - 3.0f * m_scaleY,
                          9.0f * m_scaleX, armH * 0.45f + 6.0f * m_scaleY),
                   skinColor.darker(120));
        p.fillRect(QRectF(-bodyW * 0.5f, bodyTop + bodyH * 0.25f, armW, armH * 0.4f), skinColor);
    } else if (actionState == ActorActionState::LightAttack || actionState == ActorActionState::HeavyAttack) {
        const float extend = (actionState == ActorActionState::HeavyAttack) ? armW * 3.0f : armW * 2.0f;
        p.fillRect(QRectF(bodyW * 0.2f, bodyTop + bodyH * 0.15f, extend, armH * 0.5f),
                   skinColor);
        p.fillRect(QRectF(bodyW * 0.2f + extend - 4.0f * m_scaleX,
                          bodyTop + bodyH * 0.15f - 3.0f * m_scaleY,
                          8.0f * m_scaleX, armH * 0.5f + 6.0f * m_scaleY),
                   skinColor.darker(120));
        p.fillRect(QRectF(-bodyW * 0.5f, bodyTop + bodyH * 0.3f, armW, armH * 0.45f), skinColor);
    } else if (actionState == ActorActionState::AirAttack) {
        p.fillRect(QRectF(bodyW * 0.1f, legTop, legW * 2.5f, legH * 0.4f), pantsColor);
        p.fillRect(QRectF(-bodyW * 0.5f, bodyTop + bodyH * 0.25f, armW, armH * 0.4f), skinColor);
    } else if (actionState == ActorActionState::Walk) {
        // 行走摆臂：与腿反相
        const float t = game_state().elapsedSeconds * 10.0f;
        const float swingFront = std::sin(t) * 4.0f * m_scaleX;
        const float swingRear  = std::sin(t + 3.14159f) * 4.0f * m_scaleX;
        p.fillRect(QRectF(bodyW * 0.3f + swingFront, bodyTop + bodyH * 0.15f, armW, armH), skinColor);
        p.fillRect(QRectF(-bodyW * 0.3f - armW + swingRear, bodyTop + bodyH * 0.15f, armW, armH), skinColor);
    } else {
        p.fillRect(QRectF(bodyW * 0.3f, bodyTop + bodyH * 0.15f, armW, armH), skinColor);
        p.fillRect(QRectF(-bodyW * 0.3f - armW, bodyTop + bodyH * 0.15f, armW, armH), skinColor);
    }

    // ---- 头部 ----
    p.setPen(Qt::NoPen);
    p.setBrush(headColor);
    p.drawEllipse(QPointF(0, bodyTop - headR * 0.3f), headR, headR);

    // 眼睛
    const float eyeX = headR * 0.35f;
    const float eyeY = bodyTop - headR * 0.4f;
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(QPointF(eyeX, eyeY), headR * 0.3f, headR * 0.35f);
    p.setBrush(QColor(20, 20, 20));
    p.drawEllipse(QPointF(eyeX + headR * 0.1f, eyeY), headR * 0.15f, headR * 0.18f);

    // 头发/头巾（根据队伍颜色）
    p.setBrush(bodyColor.darker(150));
    p.drawEllipse(QPointF(0, bodyTop - headR * 0.7f), headR * 1.05f, headR * 0.5f);

    p.setBrush(Qt::NoBrush);

    // ---- 受伤/死亡特效 ----
    if (actionState == ActorActionState::Dead) {
        p.setPen(QPen(QColor(255, 60, 60, 120), 2.0f));
        p.drawLine(QPointF(-w * 0.4f, h * 0.1f), QPointF(w * 0.4f, h * 0.5f));
        p.drawLine(QPointF(w * 0.4f, h * 0.1f), QPointF(-w * 0.4f, h * 0.5f));
    }
}

void GameWidget::drawHealthBar(QPainter& p, const ActorState& actor)
{
    const float screenX = worldToScreenX(actor.position.x);
    const float screenY = worldToScreenY(actor.position.laneY, actor.position.z);
    const float w = actor.drawSize.width * m_scaleX;
    const float h = actor.drawSize.height * m_scaleY;
    const float barW = w * 0.8f;
    const float barH = 4.0f * m_scaleY;
    const float barX = screenX - barW / 2;
    const float barY = screenY - h - barH - 4.0f * m_scaleY;
    const float ratio = actor.health.ratio();

    // 背景
    p.fillRect(QRectF(barX, barY, barW, barH), QColor(20, 20, 20, 180));
    // 填充
    p.fillRect(QRectF(barX, barY, barW * ratio, barH), healthBarColor(ratio));
    // 边框
    p.setPen(QPen(QColor(180, 180, 180, 200), 1.0f));
    p.drawRect(QRectF(barX, barY, barW, barH));
}

void GameWidget::drawPlayerStatus(QPainter& p)
{
    // 玩家身上的状态文字（例如连招计数）
    Q_UNUSED(p);
}

// ============================================================================
// HUD 绘制
// ============================================================================

void GameWidget::drawHUD(QPainter& p)
{
    const auto& hud = game_state().hud;
    const float uiScale = std::clamp(std::min(m_scaleX, m_scaleY), 0.45f, 1.5f);
    const float margin = 12.0f * uiScale;
    const auto phase = hudFrameIndex(m_goBlinkTimer);

    const auto drawMeter = [&p, phase](const std::array<QPixmap, 8>& frames,
                                       const QRectF& target,
                                       int sourceCapWidth,
                                       float ratio,
                                       float delayedRatio) {
        if (frames[phase].isNull() || frames[phase + 4u].isNull()) return false;

        const float real = std::clamp(ratio, 0.0f, 1.0f);
        const float delayed = std::clamp(std::max(delayedRatio, real), 0.0f, 1.0f);
        const auto drawStretchedFrame = [&p, &target, sourceCapWidth](const QPixmap& frame) {
            const float sourceW = static_cast<float>(frame.width());
            const float sourceH = static_cast<float>(frame.height());
            const float sourceCap = std::min(static_cast<float>(sourceCapWidth), sourceW * 0.5f);
            const float targetCap = std::min(
                static_cast<float>(target.width() * 0.5),
                std::round(sourceCap * static_cast<float>(target.height()) / sourceH));
            const float sourceMiddleW = std::max(0.0f, sourceW - 2.0f * sourceCap);
            const float targetMiddleW = std::max(
                0.0f, static_cast<float>(target.width()) - 2.0f * targetCap);

            p.drawPixmap(QRectF(target.x(), target.y(), targetCap, target.height()),
                         frame, QRectF(0.0f, 0.0f, sourceCap, sourceH));
            p.drawPixmap(QRectF(target.x() + targetCap, target.y(),
                                targetMiddleW, target.height()),
                         frame, QRectF(sourceCap, 0.0f, sourceMiddleW, sourceH));
            p.drawPixmap(QRectF(target.right() - targetCap, target.y(),
                                targetCap, target.height()),
                         frame, QRectF(sourceW - sourceCap, 0.0f, sourceCap, sourceH));
        };
        const auto drawActiveLayer = [&](float fill, float opacity) {
            if (fill <= 0.0f) return;
            p.save();
            p.setOpacity(opacity);
            p.setClipRect(QRectF(target.x(), target.y(),
                                 target.width() * fill, target.height()),
                          Qt::IntersectClip);
            drawStretchedFrame(frames[phase]);
            p.restore();
        };

        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        drawStretchedFrame(frames[phase + 4u]);
        drawActiveLayer(delayed, 0.42f);
        drawActiveLayer(real, 1.0f);
        p.restore();
        return true;
    };

    // ---- 玩家 HUD（左上） ----
    const QRectF scrollRect(margin - 18.0f * uiScale, margin,
                            40.0f * uiScale, 40.0f * uiScale);
    if (!m_hudScrollArt[phase].isNull()) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawPixmap(scrollRect, m_hudScrollArt[phase],
                     QRectF(m_hudScrollArt[phase].rect()));
        p.restore();
    }

    const QRectF healthRect(margin + 20.0f * uiScale, margin,
                            210.0f * uiScale, 34.0f * uiScale);
    const float realRatio = hud.playerHealth.ratio();
    if (!drawMeter(m_healthBarArt, healthRect, 4, realRatio,
                   m_displayHealthRatio)) {
        drawBar(p, healthRect.x(), healthRect.y(), healthRect.width(),
                healthRect.height(), realRatio, healthBarColor(realRatio), {});
    }

    const QRectF energyRect(healthRect.x() + 10.0f * uiScale,
                            healthRect.y() + 29.0f * uiScale,
                            160.0f * uiScale, 14.0f * uiScale);
    const float energyRatio = hud.playerEnergy.ratio();
    if (!drawMeter(m_energyBarArt, energyRect, 3, energyRatio, energyRatio)) {
        drawBar(p, energyRect.x(), energyRect.y(), energyRect.width(),
                energyRect.height(), energyRatio,
                energyBarColor(energyRatio), {});
    }

    const int hudFontPx = std::max(7, static_cast<int>(std::lround(9.0f * uiScale)));
    p.setPen(QColor(255, 255, 255));
    QFont hudFont("Arial", hudFontPx, QFont::Bold);
    hudFont.setPixelSize(hudFontPx);
    p.setFont(hudFont);
    const QRectF valueRect(healthRect.right() + 6.0f * uiScale,
                           healthRect.y(), 86.0f * uiScale, 36.0f * uiScale);
    const QRectF healthValueRect(valueRect.x(), valueRect.y(),
                                 valueRect.width(), valueRect.height() * 0.5f);
    const QRectF energyValueRect(valueRect.x(), healthValueRect.bottom(),
                                 valueRect.width(), valueRect.height() * 0.5f);
    p.drawText(healthValueRect, Qt::AlignLeft | Qt::AlignVCenter,
               QString("HP %1/%2")
                   .arg(hud.playerHealth.current)
                   .arg(hud.playerHealth.maximum));
    p.setPen(hud.playerExhausted ? QColor(255, 80, 80) : QColor(255, 255, 255));
    p.drawText(energyValueRect, Qt::AlignLeft | Qt::AlignVCenter,
               hud.playerExhausted
                   ? QStringLiteral("EN EXHAUSTED")
                   : QString("EN %1/%2")
                         .arg(hud.playerEnergy.current)
                         .arg(hud.playerEnergy.maximum));

    // ---- Boss 血条 (上方居中) ----
    if (hud.showBossHealth) {
        const float bossBarW = 280.0f * m_scaleX;
        const float bossBarH = 20.0f * m_scaleY;
        const float bossX = (game_state().map.viewportWidth * m_scaleX) / 2.0f - bossBarW / 2;
        const float bossY = 8.0f * m_scaleY;

        // 背景框
        p.fillRect(QRectF(bossX - 2, bossY - 2, bossBarW + 4, bossBarH + 4), QColor(0, 0, 0, 200));
        p.setPen(QPen(QColor(200, 180, 100), 2.0f));
        p.drawRect(QRectF(bossX - 1, bossY - 1, bossBarW + 2, bossBarH + 2));

        // 渐变血量条
        const float bossRatio = hud.bossHealth.ratio();
        QLinearGradient bossGrad(bossX, 0, bossX + bossBarW, 0);
        bossGrad.setColorAt(0.0, QColor(220, 50, 30));
        bossGrad.setColorAt(0.5, QColor(240, 100, 40));
        bossGrad.setColorAt(1.0, QColor(180, 20, 20));
        p.fillRect(QRectF(bossX, bossY, bossBarW, bossBarH), QColor(30, 30, 30, 220));
        p.fillRect(QRectF(bossX, bossY, bossBarW * bossRatio, bossBarH), bossGrad);

        // 低血量脉冲
        if (bossRatio < 0.3f) {
            const float pulse = 0.6f + 0.4f * std::sin(m_goBlinkTimer * 8.0f);
            p.fillRect(QRectF(bossX, bossY, bossBarW * bossRatio, bossBarH),
                       QColor(255, 60, 40, static_cast<int>(60 * pulse)));
        }

        // 数值
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.drawText(QRectF(bossX, bossY, bossBarW, bossBarH), Qt::AlignCenter,
                   QString("BOSS  %1 / %2")
                       .arg(hud.bossHealth.current)
                       .arg(hud.bossHealth.maximum));
    }

    // ---- 屏幕消息 ----
    if (!game_state().screenMessage.empty()) {
        const float msgY = game_state().map.viewportHeight * m_scaleY * 0.5f;
        const float msgX = game_state().map.viewportWidth * m_scaleX * 0.5f;

        QFont msgFont("Arial", 18, QFont::Bold);
        p.setFont(msgFont);
        p.setPen(QColor(255, 255, 100));
        p.drawText(QRectF(msgX - 150.0f * m_scaleX, msgY - 20.0f * m_scaleY,
                          300.0f * m_scaleX, 40.0f * m_scaleY),
                   Qt::AlignCenter, QString::fromStdString(game_state().screenMessage));
    }

    // ---- 关卡进度条 (底部) ----
    const float progBarW = width() * 0.6f;
    const float progBarH = 6.0f * m_scaleY;
    const float progX = (width() - progBarW) / 2.0f;
    const float progY = height() - progBarH - 8.0f * m_scaleY;
    const float progress = std::clamp(game_state().progressRatio, 0.0f, 1.0f);

    p.fillRect(QRectF(progX, progY, progBarW, progBarH), QColor(30, 30, 30, 180));
    p.fillRect(QRectF(progX, progY, progBarW * progress, progBarH),
               QColor(255, 200, 50, 200));

    p.setPen(QPen(QColor(150, 150, 150, 150), 1.0f));
    p.drawRect(QRectF(progX, progY, progBarW, progBarH));
}

// ============================================================================
// 覆盖层（标题 / 暂停 / 结算 / 胜利）
// ============================================================================

void GameWidget::drawInterfacePanel(QPainter& p, const QRectF& rect) const
{
    if (std::any_of(m_interfaceFrameArt.begin(), m_interfaceFrameArt.end(),
                    [](const QPixmap& pixmap) { return pixmap.isNull(); })) {
        p.fillRect(rect, QColor(20, 30, 58, 235));
        p.setPen(QPen(QColor(65, 205, 220), 2.0f));
        p.drawRect(rect);
        return;
    }

    const float corner = std::min({32.0f,
                                   static_cast<float>(rect.width() * 0.5),
                                   static_cast<float>(rect.height() * 0.5)});
    const float middleW = std::max(0.0f, static_cast<float>(rect.width()) - 2.0f * corner);
    const float middleH = std::max(0.0f, static_cast<float>(rect.height()) - 2.0f * corner);
    const auto drawTile = [&p, this](const QRectF& target, std::size_t index) {
        p.drawPixmap(target, m_interfaceFrameArt[index],
                     QRectF(m_interfaceFrameArt[index].rect()));
    };

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    drawTile(QRectF(rect.x() + corner, rect.y() + corner, middleW, middleH), 4);
    drawTile(QRectF(rect.x() + corner, rect.y(), middleW, corner), 1);
    drawTile(QRectF(rect.x() + corner, rect.bottom() - corner, middleW, corner), 7);
    drawTile(QRectF(rect.x(), rect.y() + corner, corner, middleH), 3);
    drawTile(QRectF(rect.right() - corner, rect.y() + corner, corner, middleH), 5);
    drawTile(QRectF(rect.x(), rect.y(), corner, corner), 0);
    drawTile(QRectF(rect.right() - corner, rect.y(), corner, corner), 2);
    drawTile(QRectF(rect.x(), rect.bottom() - corner, corner, corner), 6);
    drawTile(QRectF(rect.right() - corner, rect.bottom() - corner, corner, corner), 8);
    p.restore();
}

void GameWidget::drawInterfaceButton(QPainter& p, const QRectF& rect,
                                     const std::array<QPixmap, 6>& art,
                                     bool bright) const
{
    const std::size_t offset = bright ? 0u : 3u;
    if (art[offset].isNull() || art[offset + 1u].isNull() ||
        art[offset + 2u].isNull()) {
        p.fillRect(rect, bright ? QColor(30, 170, 130) : QColor(35, 65, 85));
        return;
    }

    const float cap = std::min(static_cast<float>(rect.height()),
                               static_cast<float>(rect.width() * 0.5));
    const float middleW = std::max(0.0f, static_cast<float>(rect.width()) - 2.0f * cap);
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawPixmap(QRectF(rect.x(), rect.y(), cap, rect.height()),
                 art[offset], QRectF(art[offset].rect()));
    p.drawPixmap(QRectF(rect.x() + cap, rect.y(), middleW, rect.height()),
                 art[offset + 1u], QRectF(art[offset + 1u].rect()));
    p.drawPixmap(QRectF(rect.right() - cap, rect.y(), cap, rect.height()),
                 art[offset + 2u], QRectF(art[offset + 2u].rect()));
    p.restore();
}

void GameWidget::drawOverlay(QPainter& p)
{
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    const GamePhase phase = game_state().phase;
    p.fillRect(rect(), QColor(2, 5, 12, phase == GamePhase::Title ? 155 : 205));

    const float uniformScale = std::max(0.001f,
        std::min(static_cast<float>(width()) / vpW,
                 static_cast<float>(height()) / vpH));
    const float offsetX = (static_cast<float>(width()) - vpW * uniformScale) * 0.5f;
    const float offsetY = (static_cast<float>(height()) - vpH * uniformScale) * 0.5f;
    const float cx = vpW * 0.5f;
    const float cy = vpH * 0.5f;
    const bool bright = std::fmod(m_goBlinkTimer, 1.2f) < 0.78f;

    const auto uiFont = [this](int pixels, QFont::Weight weight) {
        QFont font(m_interfaceFontFamily);
        font.setPixelSize(pixels);
        font.setWeight(weight);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
        return font;
    };
    const auto drawPixmap = [&p](const QPixmap& pixmap, const QRectF& target) {
        if (!pixmap.isNull()) p.drawPixmap(target, pixmap, QRectF(pixmap.rect()));
    };
    const auto drawSlotText = [&p](const QRectF& slot, const QString& text) {
        p.save();
        p.setClipRect(slot);
        p.drawText(slot, Qt::AlignCenter | Qt::TextSingleLine, text);
        p.restore();
    };

    p.save();
    p.translate(offsetX, offsetY);
    p.scale(uniformScale, uniformScale);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::TextAntialiasing, false);

    if (phase == GamePhase::Title) {
        const float bannerW = std::min(540.0f, vpW - 72.0f);
        const float bannerH = bannerW * 115.0f / 413.0f;
        const QRectF banner(cx - bannerW * 0.5f, 54.0f, bannerW, bannerH);
        drawPixmap(m_interfaceLogoArt[2], banner);

        p.setFont(uiFont(46, QFont::ExtraBold));
        p.setPen(QColor(246, 239, 211));
        p.drawText(QRectF(banner.x() + 34.0f, banner.y() + 8.0f,
                          banner.width() - 68.0f, banner.height() * 0.58f),
                   Qt::AlignCenter, "ALLEY FIST");
        p.setFont(uiFont(10, QFont::Bold));
        p.setPen(QColor(69, 222, 226));
        drawSlotText(QRectF(banner.x() + 46.0f,
                            banner.y() + banner.height() * (91.0f / 115.0f),
                            banner.width() - 92.0f,
                            banner.height() * (20.0f / 115.0f)),
                     QStringLiteral("NEON STREET BRAWLER"));

        p.setFont(uiFont(13, QFont::Bold));
        p.setPen(QColor(171, 192, 206));
        p.drawText(QRectF(cx - 180.0f, 330.0f, 360.0f, 28.0f),
                   Qt::AlignCenter, "STAGE 01  //  NIGHT DISTRICT");

        const QRectF startButton(cx - 145.0f, 394.0f, 290.0f, 48.0f);
        drawInterfaceButton(p, startButton, m_greenButtonArt, bright);
        drawPixmap(m_startIcon, QRectF(startButton.x() + 12.0f,
                                       startButton.y() + 8.0f, 32.0f, 32.0f));
        p.setFont(uiFont(19, QFont::ExtraBold));
        p.setPen(QColor(244, 250, 231));
        p.drawText(startButton.adjusted(48.0f, 0.0f, -12.0f, 0.0f),
                   Qt::AlignCenter, "START");
    } else if (phase == GamePhase::Paused) {
        const QRectF panel(cx - 210.0f, cy - 132.0f, 420.0f, 264.0f);
        drawInterfacePanel(p, panel);
        const QRectF banner(cx - 142.5f, cy - 105.0f, 285.0f, 115.0f);
        drawPixmap(m_interfaceLogoArt[0], banner);
        drawPixmap(m_pauseIcon, QRectF(banner.x() + 22.0f,
                                       banner.y() + 24.0f, 38.0f, 38.0f));
        p.setFont(uiFont(32, QFont::ExtraBold));
        p.setPen(QColor(242, 246, 232));
        p.drawText(QRectF(banner.x() + 58.0f, banner.y() + 8.0f,
                          banner.width() - 76.0f, 64.0f),
                   Qt::AlignCenter, "PAUSED");
        p.setFont(uiFont(9, QFont::Bold));
        p.setPen(QColor(76, 213, 224));
        drawSlotText(QRectF(banner.x() + 34.0f, banner.y() + 91.0f,
                            banner.width() - 68.0f, 20.0f),
                     QStringLiteral("SESSION SUSPENDED"));

        const QRectF resumeButton(cx - 120.0f, cy + 68.0f, 240.0f, 44.0f);
        drawInterfaceButton(p, resumeButton, m_cyanButtonArt, bright);
        p.setFont(uiFont(17, QFont::ExtraBold));
        p.setPen(QColor(241, 250, 240));
        p.drawText(resumeButton, Qt::AlignCenter, "RESUME");
    } else if (phase == GamePhase::GameOver || phase == GamePhase::Win) {
        const bool won = phase == GamePhase::Win;
        const auto& result = game_state().result;
        const QRectF panel(cx - 235.0f, cy - 155.0f, 470.0f, 310.0f);
        drawInterfacePanel(p, panel);

        const QRectF banner(cx - 174.5f, cy - 133.0f, 349.0f, 115.0f);
        drawPixmap(m_interfaceLogoArt[1], banner);
        const QPixmap& resultIcon = won ? m_winIcon : m_lossIcon;
        drawPixmap(resultIcon, QRectF(banner.x() + 24.0f,
                                      banner.y() + 23.0f, 42.0f, 42.0f));
        p.setFont(uiFont(30, QFont::ExtraBold));
        p.setPen(won ? QColor(239, 216, 79) : QColor(255, 93, 75));
        p.drawText(QRectF(banner.x() + 64.0f, banner.y() + 8.0f,
                          banner.width() - 84.0f, 64.0f),
                   Qt::AlignCenter, won ? "VICTORY" : "GAME OVER");
        p.setFont(uiFont(9, QFont::Bold));
        p.setPen(QColor(75, 216, 224));
        drawSlotText(QRectF(banner.x() + 42.0f, banner.y() + 91.0f,
                            banner.width() - 84.0f, 20.0f),
                     won ? QStringLiteral("MISSION COMPLETE")
                         : QStringLiteral("RUN TERMINATED"));

        p.setFont(uiFont(15, QFont::Bold));
        p.setPen(QColor(221, 230, 224));
        p.drawText(QRectF(cx - 160.0f, cy + 2.0f, 320.0f, 28.0f),
                   Qt::AlignCenter,
                   QString("TIME  %1 SEC").arg(result.elapsedSeconds, 0, 'f', 1));
        p.drawText(QRectF(cx - 160.0f, cy + 34.0f, 320.0f, 28.0f),
                   Qt::AlignCenter,
                   QString("DEFEATED  %1").arg(result.defeatedEnemies));

        const QRectF restartButton(cx - 122.0f, cy + 88.0f, 244.0f, 44.0f);
        drawInterfaceButton(p, restartButton,
                            won ? m_greenButtonArt : m_redButtonArt, bright);
        drawPixmap(m_restartIcon, QRectF(restartButton.x() + 9.0f,
                                         restartButton.y() + 6.0f, 32.0f, 32.0f));
        p.setFont(uiFont(16, QFont::ExtraBold));
        p.setPen(QColor(247, 246, 231));
        p.drawText(restartButton.adjusted(42.0f, 0.0f, -8.0f, 0.0f),
                   Qt::AlignCenter, "RESTART");
    }

    p.restore();
}

// ============================================================================
// 遭遇战锁屏与 Boss 出场动画
// ============================================================================

void GameWidget::drawEncounterOverlay(QPainter& p)
{
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;
    const auto& enc = game_state().encounter;
    const float uniformScale = std::max(0.001f,
        std::min(static_cast<float>(width()) / vpW,
                 static_cast<float>(height()) / vpH));
    const float offsetX = (static_cast<float>(width()) - vpW * uniformScale) * 0.5f;
    const float offsetY = (static_cast<float>(height()) - vpH * uniformScale) * 0.5f;
    const float cx = vpW * 0.5f;

    const auto uiFont = [this](int pixels, QFont::Weight weight) {
        QFont font(m_interfaceFontFamily);
        font.setPixelSize(pixels);
        font.setWeight(weight);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
        return font;
    };
    const auto drawPixmap = [&p](const QPixmap& pixmap, const QRectF& target) {
        if (!pixmap.isNull()) p.drawPixmap(target, pixmap, QRectF(pixmap.rect()));
    };

    p.save();
    p.translate(offsetX, offsetY);
    p.scale(uniformScale, uniformScale);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::TextAntialiasing, false);

    if (enc.kind == EncounterKind::EnemyWave && enc.remainingEnemies > 0 &&
        enc.phase == EncounterPhase::Fighting) {
        p.setPen(QColor(255, 200, 100));
        p.setFont(uiFont(12, QFont::Bold));
        p.drawText(QRectF(cx - 90.0f, vpH - 30.0f, 180.0f, 22.0f),
                   Qt::AlignCenter,
                   QString("ENEMIES  %1").arg(enc.remainingEnemies));
    }

    if (enc.kind != EncounterKind::Boss || enc.phase != EncounterPhase::Intro) {
        p.restore();
        return;
    }

    const float introProgress = std::clamp(enc.introProgress, 0.0f, 1.0f);
    constexpr float warningEnd = 0.38f;
    constexpr float revealEnd = 0.92f;
    const float reveal = std::clamp((introProgress - warningEnd) /
                                        (revealEnd - warningEnd),
                                    0.0f, 1.0f);
    const bool initialWarning = introProgress < warningEnd;
    const float barH = vpH * 0.44f * (initialWarning ? 1.0f : 1.0f - reveal);
    p.fillRect(QRectF(0.0f, 0.0f, vpW, barH), QColor(0, 0, 0, 228));
    p.fillRect(QRectF(0.0f, vpH - barH, vpW, barH), QColor(0, 0, 0, 228));

    const float bannerW = 540.0f;
    const float bannerH = bannerW * 115.0f / 413.0f;
    const float bannerY = 166.0f + (initialWarning ? 0.0f : (1.0f - reveal) * -72.0f);
    const QRectF banner(cx - bannerW * 0.5f, bannerY, bannerW, bannerH);
    const float pulse = 0.72f + 0.28f * std::abs(std::sin(m_goBlinkTimer * 10.0f));

    p.save();
    p.setOpacity(initialWarning ? pulse : std::max(0.35f, reveal));
    drawPixmap(m_interfaceLogoArt[2], banner);
    drawPixmap(m_bossIcon, QRectF(banner.x() + 22.0f,
                                  banner.y() + 24.0f, 42.0f, 42.0f));
    drawPixmap(m_bossIcon, QRectF(banner.right() - 64.0f,
                                  banner.y() + 24.0f, 42.0f, 42.0f));
    p.setFont(uiFont(initialWarning ? 36 : 28, QFont::ExtraBold));
    p.setPen(initialWarning ? QColor(255, 80, 58) : QColor(247, 222, 107));
    p.drawText(QRectF(banner.x() + 68.0f, banner.y() + 8.0f,
                      banner.width() - 136.0f, 64.0f),
               Qt::AlignCenter,
               initialWarning ? "WARNING" : "THE EXECUTIONER");
    p.setFont(uiFont(10, QFont::Bold));
    p.setPen(QColor(71, 222, 225));
    const QRectF statusSlot(banner.x() + 52.0f,
                            banner.y() + banner.height() * (91.0f / 115.0f),
                            banner.width() - 104.0f,
                            banner.height() * (20.0f / 115.0f));
    p.save();
    p.setClipRect(statusSlot);
    p.drawText(statusSlot, Qt::AlignCenter | Qt::TextSingleLine,
               initialWarning ? QStringLiteral("HOSTILE SIGNAL DETECTED")
                              : QStringLiteral("BOSS APPROACHING"));
    p.restore();
    p.restore();

    if (reveal >= 1.0f) {
        const float edgePulse = 0.5f + 0.5f * std::sin(m_goBlinkTimer * 6.0f);
        const QColor edgeColor(235, 44, 32, static_cast<int>(190 * edgePulse));
        p.fillRect(QRectF(0.0f, 0.0f, vpW, 8.0f), edgeColor);
        p.fillRect(QRectF(0.0f, vpH - 8.0f, vpW, 8.0f), edgeColor);
    }

    p.restore();
}

// ============================================================================
// 飞行道具
// ============================================================================

void GameWidget::drawProjectile(QPainter& p, const ProjectileState& proj)
{
    const float sx = worldToScreenX(proj.position.x);
    const float sy = worldToScreenY(proj.position.laneY, proj.position.z);

    if (proj.visualVariant == ActorVisualVariant::RangedRobot &&
        !m_robotProjectile.isNull()) {
        const float projectileW = 22.0f * m_scaleX;
        const float projectileH = 8.0f * m_scaleY;
        const float direction = proj.facing == Facing::Right ? 1.0f : -1.0f;

        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.translate(sx, sy);
        if (direction < 0.0f) p.scale(-1.0f, 1.0f);
        p.drawPixmap(QRectF(-projectileW * 0.5f, -projectileH * 0.5f,
                            projectileW, projectileH),
                     m_robotProjectile, QRectF(m_robotProjectile.rect()));
        if (!m_robotProjectileAlt.isNull()) {
            const float sparkSize = 8.0f * std::min(m_scaleX, m_scaleY);
            const float pulse = 0.55f + 0.45f * std::abs(std::sin(m_goBlinkTimer * 18.0f));
            p.setOpacity(pulse);
            p.drawPixmap(QRectF(-projectileW * 0.75f, -sparkSize * 0.5f,
                                sparkSize, sparkSize),
                         m_robotProjectileAlt,
                         QRectF(m_robotProjectileAlt.rect()));
        }
        p.restore();
        return;
    }

    const float r = 5.0f * m_scaleX;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 221, 70));
    p.drawRect(QRectF(sx - r * 1.6f, sy - r * 0.4f,
                      r * 3.2f, r * 0.8f));
    p.setBrush(QColor(255, 245, 180, 150));
    p.drawRect(QRectF(sx - r * 0.8f, sy - r * 0.7f,
                      r * 1.6f, r * 1.4f));
}

// ============================================================================
// 掉落物
// ============================================================================

void GameWidget::drawPickup(QPainter& p, const PickupState& pickup)
{
    const float sx = worldToScreenX(pickup.position.x);
    const float sy = worldToScreenY(pickup.position.laneY, 20.0f);
    const float r = 8.0f * m_scaleX;
    const float hover = std::sin(game_state().elapsedSeconds * 2.0f + pickup.id * 0.7f) * 1.5f * m_scaleY;

    p.setPen(QPen(QColor(255, 255, 255, 150), 1.5f));
    if (pickup.kind == PickupKind::Health) {
        p.setBrush(QColor(220, 50, 50, 200));
        p.drawEllipse(QPointF(sx, sy + hover), r, r);
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.drawText(QRectF(sx - r, sy + hover - r, r * 2, r * 2), Qt::AlignCenter, "+");
    } else {
        p.setBrush(QColor(50, 120, 220, 200));
        p.drawEllipse(QPointF(sx, sy + hover), r, r);
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.drawText(QRectF(sx - r, sy + hover - r, r * 2, r * 2), Qt::AlignCenter, "E");
    }
}

// ============================================================================
// 粒子特效
// ============================================================================

void GameWidget::drawParticles(QPainter& p)
{
    // 敌人受击时生成打击粒子（仅在 Hurt 状态瞬间）
    for (const auto& enemy : game_state().enemies) {
        if (enemy.actionState == ActorActionState::Hurt) {
            const float sx = worldToScreenX(enemy.position.x);
            const float sy = worldToScreenY(enemy.position.laneY, 0);
            const int count = (enemy.lastImpact == ImpactLevel::Heavy) ? 6 : 3;

            p.setPen(Qt::NoPen);
            for (int i = 0; i < count; ++i) {
                const float angle = static_cast<float>(i) / static_cast<float>(count) * 6.28f + m_goBlinkTimer;
                const float dist = (8.0f + i * 3.0f) * m_scaleX;
                const float px = sx + std::cos(angle) * dist;
                const float py = sy - 20.0f * m_scaleY + std::sin(angle) * dist;
                const float pr = 2.5f * m_scaleX;
                p.setBrush(QColor(255, 220, 60, 180));
                p.drawEllipse(QPointF(px, py), pr, pr);
            }
        }
    }
}

} // namespace alleyfist
