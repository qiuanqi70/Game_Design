#include "GameWidget.h"

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
            }
        }
        if (m_screenShakeTimer > 0.0f) {
            m_screenShakeTimer = std::max(0.0f, m_screenShakeTimer - clampedDt);
        }

        // Boss 出场动画计时
        if (m_gameState && m_gameState->encounter.kind == EncounterKind::Boss &&
            m_gameState->encounter.phase == EncounterPhase::Intro) {
            m_bossIntroAnimTimer += clampedDt;
        } else if (m_gameState && m_gameState->encounter.phase != EncounterPhase::Intro) {
            m_bossIntroAnimTimer = 0.0f;
        }

        if (m_tickCommand) m_tickCommand(clampedDt, m_frameIndex);
    });

    // 启动定时器
    m_elapsed.start();
    m_timer.start(16); // ~60 FPS
}

void GameWidget::set_game_state(const GameState* state) noexcept
{
    m_gameState = state;
    update(); //qt的内置函数，触发 paintEvent
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
        if (m_primaryActionCommand) m_primaryActionCommand();
        break;
    case Qt::Key_K: case Qt::Key_X:
        if (m_secondaryActionCommand) m_secondaryActionCommand();
        break;
    case Qt::Key_Space:
        if (m_stateToggleCommand) m_stateToggleCommand();
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
        drawOverlay(p);
        return;
    }

    // ---- 屏幕震动偏移 ----
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

    // ---- 遭遇战锁屏/出场动画 ----
    drawEncounterOverlay(p);

    // ---- HUD ----
    drawHUD(p);

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

    // 视差滚动：远景移动速度为镜头的 0.3 倍
    const float parallaxFactor = 0.3f;

    // 绘制一系列建筑
    struct Building {
        float x;
        float width;
        float height;
        QColor color;
    };

    // 固定建筑布局
    const Building buildings[] = {
        { 100.0f,  120.0f, 180.0f, QColor(50, 45, 55)  },
        { 280.0f,  90.0f,  140.0f, QColor(55, 50, 60)  },
        { 450.0f,  150.0f, 200.0f, QColor(45, 40, 50)  },
        { 680.0f,  100.0f, 160.0f, QColor(52, 47, 57)  },
        { 850.0f,  130.0f, 190.0f, QColor(48, 43, 53)  },
        { 1050.0f, 110.0f, 150.0f, QColor(55, 48, 58)  },
        { 1250.0f, 140.0f, 210.0f, QColor(42, 38, 48)  },
        { 1480.0f, 100.0f, 170.0f, QColor(50, 45, 55)  },
        { 1680.0f, 160.0f, 195.0f, QColor(47, 42, 52)  },
        { 1900.0f, 120.0f, 155.0f, QColor(53, 48, 58)  },
        { 2100.0f, 135.0f, 185.0f, QColor(44, 40, 50)  },
        { 2350.0f, 145.0f, 205.0f, QColor(49, 44, 54)  },
        { 2600.0f, 115.0f, 165.0f, QColor(51, 46, 56)  },
        { 2800.0f, 125.0f, 175.0f, QColor(46, 41, 51)  },
    };

    for (const auto& b : buildings) {
        // 视差位置
        const float parallaxX = b.x - camX * parallaxFactor;
        const float screenX = parallaxX * m_scaleX;
        const float screenW = b.width * m_scaleX;
        const float bottomY = streetTop * m_scaleY;
        const float screenH = b.height * m_scaleY;
        const float screenY = bottomY - screenH;

        // 裁剪：只画在屏幕内的建筑
        if (screenX + screenW < 0 || screenX > width()) continue;

        // 建筑主体
        p.fillRect(QRectF(screenX, screenY, screenW, screenH), b.color);

        // 窗户（简单网格）
        p.setPen(Qt::NoPen);
        const QColor winLit(255, 220, 100, 80);   // 亮窗
        const QColor winDark(30, 25, 35, 120);    // 暗窗
        const float winW = 12.0f * m_scaleX;
        const float winH = 14.0f * m_scaleY;
        const float winGapX = 20.0f * m_scaleX;
        const float winGapY = 24.0f * m_scaleY;
        const float marginX = 8.0f * m_scaleX;
        const float marginY = 10.0f * m_scaleY;

        for (float wy = screenY + marginY; wy + winH < bottomY; wy += winGapY) {
            for (float wx = screenX + marginX; wx + winW < screenX + screenW; wx += winGapX) {
                // 随机亮/暗（用位置哈希决定）
                const int hash = (static_cast<int>(wx * 100) + static_cast<int>(wy * 200)) % 3;
                p.fillRect(QRectF(wx, wy, winW, winH),
                           (hash == 0) ? winLit : winDark);
            }
        }
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

void GameWidget::drawActor(QPainter& p, const ActorState& actor)
{
    const float screenX = worldToScreenX(actor.position.x);
    const float screenY = worldToScreenY(actor.position.laneY, actor.position.z);

    const float w = actor.drawSize.width * m_scaleX;
    const float h = actor.drawSize.height * m_scaleY;

    // 绘制矩形范围的中心
    const float cx = screenX;
    const float cy = screenY - h; // 脚底 = laneY，头顶 = laneY - drawSize.height

    // 裁剪：不在屏幕内的跳过
    if (cx + w / 2 < -50 || cx - w / 2 > width() + 50 ||
        cy + h < -50 || cy > height() + 50) {
        return;
    }

    const QColor bodyColor = actorBodyColor(actor.team, actor.kind);

    p.save();
    p.translate(cx, cy);

    // 根据 facing 翻转
    if (actor.facing == Facing::Left) {
        p.scale(-1.0f, 1.0f);
    }

    // 受击闪白（仅在 Hurt 状态瞬间显示）
    if (actor.actionState == ActorActionState::Hurt) {
        p.fillRect(QRectF(-w * 0.5f, 0, w, h), QColor(255, 255, 255, 140));
    }

    // 根据状态绘制不同姿势
    drawCharacterBody(p, actor, bodyColor);

    p.restore();

    // 血条（敌人和 Boss 头顶）
    if (actor.team == Team::Enemy && actor.health.maximum > 0) {
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
    if (actionState == ActorActionState::Jump || actionState == ActorActionState::AirAttack) {
        p.fillRect(QRectF(-legW * 0.5f, legTop, legW, legH * 0.5f), pantsColor);
        p.fillRect(QRectF(legW * 0.3f, legTop, legW, legH * 0.5f), pantsColor);
    } else if (actionState == ActorActionState::Walk) {
        const float t = game_state().elapsedSeconds * 10.0f;
        const float stride = std::sin(t) * 7.0f * m_scaleX;
        const float kneeBend = std::abs(std::cos(t)) * 3.0f * m_scaleY;
        // 前后腿（交替）
        p.fillRect(QRectF(stride - legW * 0.5f, legTop + kneeBend, legW, legH - kneeBend), pantsColor);
        p.fillRect(QRectF(-stride + legW * 0.3f, legTop - kneeBend * 0.5f, legW, legH + kneeBend * 0.5f), pantsColor);
        // 鞋子
        p.fillRect(QRectF(stride - legW * 0.5f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
        p.fillRect(QRectF(-stride + legW * 0.3f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
    } else {
        p.fillRect(QRectF(-legW * 0.5f, legTop, legW, legH), pantsColor);
        p.fillRect(QRectF(legW * 0.3f, legTop, legW, legH), pantsColor);
        p.fillRect(QRectF(-legW * 0.5f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
        p.fillRect(QRectF(legW * 0.3f - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
    }

    // ---- 身体 ----
    p.fillRect(QRectF(-bodyW / 2, bodyTop, bodyW, bodyH), bodyColor);

    // 衣服纹理线
    p.setPen(QPen(darkerBody, 1.0f));
    p.drawLine(QPointF(-bodyW * 0.3f, bodyTop + bodyH * 0.3f),
               QPointF(bodyW * 0.3f, bodyTop + bodyH * 0.3f));

    // ---- 手臂 ----
    if (actionState == ActorActionState::LightAttack || actionState == ActorActionState::HeavyAttack) {
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
    const float margin = 12.0f;
    const float barW = 200.0f * m_scaleX;
    const float barH = 16.0f * m_scaleY;
    const float gap = 6.0f * m_scaleY;

    // ---- 血量条 (左上) ----
    const float hpX = margin * m_scaleX;
    const float hpY = margin * m_scaleY;

    // 标签
    p.setPen(QColor(255, 255, 255));
    QFont labelFont("Arial", 10, QFont::Bold);
    p.setFont(labelFont);
    p.drawText(QPointF(hpX, hpY + barH * 0.8f), "HP");

    const float barStartX = hpX + 28.0f * m_scaleX;
    // 延迟血条（背景追尾）
    p.fillRect(QRectF(barStartX, hpY, barW, barH), QColor(30, 30, 30, 200));
    p.fillRect(QRectF(barStartX, hpY, barW * m_displayHealthRatio, barH), QColor(180, 180, 180, 120));
    const float realRatio = hud.playerHealth.ratio();
    p.fillRect(QRectF(barStartX, hpY, barW * realRatio, barH), healthBarColor(realRatio));
    p.setPen(QPen(QColor(200, 200, 200), 1.0f));
    p.drawRect(QRectF(barStartX, hpY, barW, barH));
    p.setPen(QColor(255, 255, 255));
    p.setFont(QFont("Arial", 9));
    p.drawText(QRectF(barStartX, hpY, barW, barH), Qt::AlignCenter,
               QString("%1 / %2").arg(hud.playerHealth.current).arg(hud.playerHealth.maximum));

    // ---- 精力条 (HP 下方) ----
    const float enY = hpY + barH + gap;
    p.drawText(QPointF(hpX, enY + barH * 0.8f), "EN");

    drawBar(p, barStartX, enY, barW, barH, hud.playerEnergy.ratio(),
            energyBarColor(hud.playerEnergy.ratio()),
            QString("%1 / %2").arg(hud.playerEnergy.current).arg(hud.playerEnergy.maximum));

    // ---- 精力耗尽警告 ----
    if (hud.playerExhausted) {
        const float warnY = enY + barH + gap;
        p.setPen(QColor(255, 80, 80));
        QFont warnFont("Arial", 11, QFont::Bold);
        p.setFont(warnFont);
        p.drawText(QPointF(barStartX, warnY + barH * 0.8f),
                   QString::fromUtf8("EXHAUSTED"));
    }

    // ---- Boss 血条 (上方居中) ----
    if (hud.showBossHealth) {
        const float bossBarW = 280.0f * m_scaleX;
        const float bossBarH = 20.0f * m_scaleY;
        const float bossX = (game_state().map.viewportWidth * m_scaleX) / 2.0f - bossBarW / 2;
        const float bossY = margin * m_scaleY - 4.0f * m_scaleY;

        // 背景框
        p.fillRect(QRectF(bossX - 2, bossY - 2, bossBarW + 4, bossBarH + 4), QColor(0, 0, 0, 200));
        p.setPen(QPen(QColor(200, 180, 100), 2.0f));
        p.drawRect(QRectF(bossX - 1, bossY - 1, bossBarW + 2, bossBarH + 2));

        // Boss 名字
        p.setPen(QColor(255, 200, 100));
        QFont bossFont("Arial", 11, QFont::Bold);
        p.setFont(bossFont);
        p.drawText(QPointF(bossX, bossY - 6.0f * m_scaleY), "BOSS");

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
                   QString("%1 / %2").arg(hud.bossHealth.current).arg(hud.bossHealth.maximum));
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

void GameWidget::drawOverlay(QPainter& p)
{
    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;

    // 半透明黑色遮罩
    p.fillRect(QRectF(0, 0, vpW * m_scaleX, vpH * m_scaleY),
               QColor(0, 0, 0, 180));

    const float cx = vpW * m_scaleX / 2.0f;
    const float cy = vpH * m_scaleY / 2.0f;

    const GamePhase phase = game_state().phase;

    if (phase == GamePhase::Title) {
        // ---- 标题画面 ----
        // 标题背景
        QLinearGradient titleBg(0, 0, 0, vpH * m_scaleY);
        titleBg.setColorAt(0.0, QColor(20, 15, 40));
        titleBg.setColorAt(0.5, QColor(40, 25, 60));
        titleBg.setColorAt(1.0, QColor(15, 10, 30));
        p.fillRect(QRectF(0, 0, vpW * m_scaleX, vpH * m_scaleY), titleBg);

        // 游戏标题
        QFont titleFont("Arial", 48, QFont::Bold);
        p.setFont(titleFont);

        // 文字阴影
        p.setPen(QColor(0, 0, 0, 150));
        p.drawText(QRectF(cx - 250.0f * m_scaleX + 3.0f, cy - 100.0f * m_scaleY + 3.0f,
                          500.0f * m_scaleX, 80.0f * m_scaleY),
                   Qt::AlignCenter, "ALLEY FIST");

        // 渐变文字
        QLinearGradient textGrad(cx - 200, cy - 100, cx + 200, cy - 20);
        textGrad.setColorAt(0.0, QColor(255, 200, 50));
        textGrad.setColorAt(0.5, QColor(255, 140, 30));
        textGrad.setColorAt(1.0, QColor(255, 80, 20));
        p.setPen(QPen(textGrad, 1.0f));
        p.drawText(QRectF(cx - 250.0f * m_scaleX, cy - 100.0f * m_scaleY,
                          500.0f * m_scaleX, 80.0f * m_scaleY),
                   Qt::AlignCenter, "ALLEY FIST");

        // 副标题
        QFont subFont("Arial", 16);
        p.setFont(subFont);
        p.setPen(QColor(200, 180, 150));
        p.drawText(QRectF(cx - 200.0f * m_scaleX, cy - 30.0f * m_scaleY,
                          400.0f * m_scaleX, 30.0f * m_scaleY),
                   Qt::AlignCenter, QString::fromUtf8("— 巷战双截龙 —"));

        // 操作说明
        QFont ctrlFont("Arial", 10);
        p.setFont(ctrlFont);
        p.setPen(QColor(180, 180, 180));
        const float infoY = cy + 30.0f * m_scaleY;

        struct CtrlInfo {
            QString key;
            QString action;
        };
        const CtrlInfo ctrls[] = {
            {"← ↑ ↓ → / WASD", QString::fromUtf8("移动")},
            {"J / Z", QString::fromUtf8("轻攻击")},
            {"K / X", QString::fromUtf8("重攻击")},
            {"Space", QString::fromUtf8("跳跃")},
            {"P / Esc", QString::fromUtf8("暂停")},
        };

        for (int i = 0; i < 5; ++i) {
            const float lineY = infoY + i * 22.0f * m_scaleY;
            p.setPen(QColor(255, 220, 180));
            p.drawText(QRectF(cx - 180.0f * m_scaleX, lineY,
                              160.0f * m_scaleX, 20.0f * m_scaleY),
                       Qt::AlignRight, ctrls[i].key);
            p.setPen(QColor(180, 180, 180));
            p.drawText(QRectF(cx + 20.0f * m_scaleX, lineY,
                              160.0f * m_scaleX, 20.0f * m_scaleY),
                       Qt::AlignLeft, ctrls[i].action);
        }

        // 闪烁 "Press Enter"
        const float blinkCycle = std::fmod(m_goBlinkTimer, 1.2f);
        if (blinkCycle < 0.8f) {
            QFont startFont("Arial", 18, QFont::Bold);
            p.setFont(startFont);
            p.setPen(QColor(255, 255, 200));
            p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 140.0f * m_scaleY,
                              300.0f * m_scaleX, 30.0f * m_scaleY),
                       Qt::AlignCenter, "Press ENTER to Start");
        }

    } else if (phase == GamePhase::Paused) {
        // ---- 暂停 ----
        QFont pauseFont("Arial", 36, QFont::Bold);
        p.setFont(pauseFont);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy - 40.0f * m_scaleY,
                          300.0f * m_scaleX, 50.0f * m_scaleY),
                   Qt::AlignCenter, "PAUSED");

        QFont hintFont("Arial", 14);
        p.setFont(hintFont);
        p.setPen(QColor(200, 200, 200));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 10.0f * m_scaleY,
                          300.0f * m_scaleX, 30.0f * m_scaleY),
                   Qt::AlignCenter, "Press P or ESC to Resume");

    } else if (phase == GamePhase::GameOver) {
        // ---- 游戏结束 ----
        QFont goFont("Arial", 42, QFont::Bold);
        p.setFont(goFont);
        p.setPen(QColor(220, 40, 40));
        p.drawText(QRectF(cx - 200.0f * m_scaleX, cy - 80.0f * m_scaleY,
                          400.0f * m_scaleX, 60.0f * m_scaleY),
                   Qt::AlignCenter, "GAME OVER");

        // 统计信息
        const auto& result = game_state().result;
        QFont statFont("Arial", 13);
        p.setFont(statFont);
        p.setPen(QColor(220, 220, 220));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy - 10.0f * m_scaleY,
                          300.0f * m_scaleX, 25.0f * m_scaleY),
                   Qt::AlignCenter,
                   QString("Survived: %1s").arg(result.elapsedSeconds, 0, 'f', 1));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 15.0f * m_scaleY,
                          300.0f * m_scaleX, 25.0f * m_scaleY),
                   Qt::AlignCenter,
                   QString("Defeated: %1").arg(result.defeatedEnemies));

        // 闪烁提示
        const float blinkCycle = std::fmod(m_goBlinkTimer, 1.0f);
        if (blinkCycle < 0.6f) {
            QFont restartFont("Arial", 16, QFont::Bold);
            p.setFont(restartFont);
            p.setPen(QColor(255, 220, 100));
            p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 50.0f * m_scaleY,
                              300.0f * m_scaleX, 30.0f * m_scaleY),
                       Qt::AlignCenter, "Press R to Restart");
        }

    } else if (phase == GamePhase::Win) {
        // ---- 胜利 ----
        QFont winFont("Arial", 42, QFont::Bold);
        p.setFont(winFont);
        QLinearGradient winGrad(cx - 150, cy - 80, cx + 150, cy - 20);
        winGrad.setColorAt(0.0, QColor(255, 220, 50));
        winGrad.setColorAt(1.0, QColor(255, 160, 20));
        p.setPen(QPen(winGrad, 1.0f));
        p.drawText(QRectF(cx - 200.0f * m_scaleX, cy - 80.0f * m_scaleY,
                          400.0f * m_scaleX, 60.0f * m_scaleY),
                   Qt::AlignCenter, "YOU WIN!");

        // 统计信息
        const auto& result = game_state().result;
        QFont statFont("Arial", 13);
        p.setFont(statFont);
        p.setPen(QColor(220, 220, 220));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy - 5.0f * m_scaleY,
                          300.0f * m_scaleX, 25.0f * m_scaleY),
                   Qt::AlignCenter,
                   QString("Time: %1s").arg(result.elapsedSeconds, 0, 'f', 1));
        p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 20.0f * m_scaleY,
                          300.0f * m_scaleX, 25.0f * m_scaleY),
                   Qt::AlignCenter,
                   QString("Enemies Defeated: %1").arg(result.defeatedEnemies));

        const float blinkCycle = std::fmod(m_goBlinkTimer, 1.0f);
        if (blinkCycle < 0.6f) {
            QFont restartFont("Arial", 16, QFont::Bold);
            p.setFont(restartFont);
            p.setPen(QColor(255, 220, 100));
            p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 55.0f * m_scaleY,
                              300.0f * m_scaleX, 30.0f * m_scaleY),
                       Qt::AlignCenter, "Press R to Restart");
        }
    }
}

// ============================================================================
// 遭遇战锁屏与 Boss 出场动画
// ============================================================================

void GameWidget::drawEncounterOverlay(QPainter& p)
{
    const auto& enc = game_state().encounter;
    if (enc.phase == EncounterPhase::None || enc.phase == EncounterPhase::Cleared) {
        return;
    }

    const float vpW = game_state().map.viewportWidth > 0.0f
                          ? game_state().map.viewportWidth : 960.0f;
    const float vpH = game_state().map.viewportHeight > 0.0f
                          ? game_state().map.viewportHeight : 540.0f;

    // 锁屏边框
    if (enc.phase == EncounterPhase::Fighting || enc.phase == EncounterPhase::Intro) {
        const float edgeW = 6.0f * m_scaleX;
        QColor edgeColor(200, 30, 30, 180);
        if (enc.kind == EncounterKind::Boss) {
            const float pulse = 0.7f + 0.3f * std::sin(m_goBlinkTimer * 4.0f);
            edgeColor = QColor(220, 50, 30, static_cast<int>(200 * pulse));
        }
        p.fillRect(QRectF(0, 0, vpW * m_scaleX, edgeW), edgeColor);
        p.fillRect(QRectF(0, vpH * m_scaleY - edgeW, vpW * m_scaleX, edgeW), edgeColor);
    }

    // 敌人剩余计数
    if (enc.remainingEnemies > 0 && enc.phase == EncounterPhase::Fighting) {
        p.setPen(QColor(255, 200, 100));
        p.setFont(QFont("Arial", 12, QFont::Bold));
        p.drawText(QRectF(vpW * m_scaleX * 0.5f - 80.0f * m_scaleX,
                          vpH * m_scaleY - 28.0f * m_scaleY,
                          160.0f * m_scaleX, 22.0f * m_scaleY),
                   Qt::AlignCenter,
                   QString("Enemies: %1").arg(enc.remainingEnemies));
    }

    // Boss 出场动画
    if (enc.kind == EncounterKind::Boss && enc.phase == EncounterPhase::Intro) {
        const float cx = vpW * m_scaleX * 0.5f;
        const float cy = vpH * m_scaleY * 0.3f;

        // 黑边收缩
        const float reveal = std::min(1.0f, m_bossIntroAnimTimer / 1.2f);
        const float barH = vpH * m_scaleY * 0.5f * (1.0f - reveal);
        p.fillRect(QRectF(0, 0, vpW * m_scaleX, barH), QColor(0, 0, 0, 220));
        p.fillRect(QRectF(0, vpH * m_scaleY - barH, vpW * m_scaleX, barH), QColor(0, 0, 0, 220));

        // WARNING 文字
        if (reveal < 1.0f) {
            const float warnPulse = 0.6f + 0.4f * std::sin(m_goBlinkTimer * 10.0f);
            p.setPen(QColor(255, 60, 30, static_cast<int>(255 * warnPulse)));
            p.setFont(QFont("Arial", 32, QFont::Bold));
            p.drawText(QRectF(cx - 160.0f * m_scaleX, cy - 30.0f * m_scaleY,
                              320.0f * m_scaleX, 60.0f * m_scaleY),
                       Qt::AlignCenter, "WARNING");
        }

        // Boss 名称
        if (reveal > 0.4f) {
            const float nameAlpha = (reveal - 0.4f) / 0.6f;
            p.setPen(QColor(255, 180, 80, static_cast<int>(255 * nameAlpha)));
            p.setFont(QFont("Arial", 18, QFont::Bold));
            p.drawText(QRectF(cx - 150.0f * m_scaleX, cy + 35.0f * m_scaleY,
                              300.0f * m_scaleX, 35.0f * m_scaleY),
                       Qt::AlignCenter, "BOSS APPROACHING");
        }
    }
}

// ============================================================================
// 飞行道具
// ============================================================================

void GameWidget::drawProjectile(QPainter& p, const ProjectileState& proj)
{
    const float sx = worldToScreenX(proj.position.x);
    const float sy = worldToScreenY(proj.position.laneY, 0);
    const float r = 5.0f * m_scaleX;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(200, 180, 60));
    p.drawEllipse(QPointF(sx, sy), r, r);
    p.setBrush(QColor(255, 240, 100, 150));
    p.drawEllipse(QPointF(sx, sy), r * 1.6f, r * 1.6f);
}

// ============================================================================
// 掉落物
// ============================================================================

void GameWidget::drawPickup(QPainter& p, const PickupState& pickup)
{
    const float sx = worldToScreenX(pickup.position.x);
    const float sy = worldToScreenY(pickup.position.laneY, 20.0f);
    const float r = 8.0f * m_scaleX;
    const float bob = std::sin(game_state().elapsedSeconds * 5.0f + pickup.id * 1.3f) * 3.0f * m_scaleY;

    p.setPen(QPen(QColor(255, 255, 255, 150), 1.5f));
    if (pickup.kind == PickupKind::Health) {
        p.setBrush(QColor(220, 50, 50, 200));
        p.drawEllipse(QPointF(sx, sy + bob), r, r);
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.drawText(QRectF(sx - r, sy + bob - r, r * 2, r * 2), Qt::AlignCenter, "+");
    } else {
        p.setBrush(QColor(50, 120, 220, 200));
        p.drawEllipse(QPointF(sx, sy + bob), r, r);
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.drawText(QRectF(sx - r, sy + bob - r, r * 2, r * 2), Qt::AlignCenter, "E");
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
