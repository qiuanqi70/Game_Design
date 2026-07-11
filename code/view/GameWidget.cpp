#include "GameWidget.h"

#include <QKeyEvent>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

namespace alleyfist {

// GameWidget 是纯 View：这里可以出现 Qt 事件、QPainter、颜色和布局，
// 但不写 AI、碰撞、伤害、刷怪等规则；这些规则通过快照从 ViewModel 单向流入。

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
        // 防止跳帧过大（例如断点调试返回后）
        const float clampedDt = std::min(dt, 0.1f);
        ++m_frameIndex;

        // GO 闪烁计时
        m_goBlinkTimer += clampedDt;

        emit tickRequested(clampedDt, m_frameIndex);
    });

    // 启动定时器
    m_elapsed.start();
    m_timer.start(16); // ~60 FPS
}

void GameWidget::updateSnapshot(const GameSnapshot& snapshot)
{
    m_snapshot = snapshot;
    update(); // 触发 paintEvent
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
    return (worldX - m_snapshot.map.cameraX) * m_scaleX;
}

float GameWidget::worldToScreenY(float laneY, float z) const
{
    // laneY 是街道纵深，z 是离地高度（跳跃时 > 0）
    return (laneY - z) * m_scaleY;
}

// ============================================================================
// 键盘输入处理
// ============================================================================

InputAction GameWidget::keyToAction(int qtKey, bool pressed) const
{
    Q_UNUSED(pressed);
    switch (qtKey) {
    case Qt::Key_Left:  case Qt::Key_A: return InputAction::MoveLeft;
    case Qt::Key_Right: case Qt::Key_D: return InputAction::MoveRight;
    case Qt::Key_Up:    case Qt::Key_W: return InputAction::MoveUp;
    case Qt::Key_Down:  case Qt::Key_S: return InputAction::MoveDown;
    case Qt::Key_J: case Qt::Key_Z:     return InputAction::LightAttack;
    case Qt::Key_K: case Qt::Key_X:     return InputAction::HeavyAttack;
    case Qt::Key_Space:                 return InputAction::Jump;
    case Qt::Key_R:                     return InputAction::Restart;
    case Qt::Key_Return: case Qt::Key_Enter: return InputAction::Confirm;
    case Qt::Key_Escape: case Qt::Key_P:     return InputAction::Pause;
    default: break;
    }
    // 返回一个不会被识别的动作，调用方应忽略。
    return InputAction::Confirm;
}

static bool isMovementAction(InputAction action)
{
    return action == InputAction::MoveLeft  ||
           action == InputAction::MoveRight ||
           action == InputAction::MoveUp    ||
           action == InputAction::MoveDown;
}

static bool isHandledActionKey(int qtKey)
{
    switch (qtKey) {
    case Qt::Key_Left:
    case Qt::Key_A:
    case Qt::Key_Right:
    case Qt::Key_D:
    case Qt::Key_Up:
    case Qt::Key_W:
    case Qt::Key_Down:
    case Qt::Key_S:
    case Qt::Key_J:
    case Qt::Key_Z:
    case Qt::Key_K:
    case Qt::Key_X:
    case Qt::Key_Space:
    case Qt::Key_R:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Escape:
    case Qt::Key_P:
        return true;
    default:
        return false;
    }
}
//判断键盘的输入类型，转换之后发给viewmodel层去处理
void GameWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        // 自动重复不产生新的 Triggered，只维持 Pressed。
        // 但 Qt 对非文本键通常不发送 autoRepeat，这里做安全处理。
        return;
    }

    const int key = event->key();
    if (!isHandledActionKey(key)) {
        QWidget::keyPressEvent(event);
        return;
    }

    const InputAction action = keyToAction(key, true);


    if (isMovementAction(action)) {
        // 移动键：只把 Pressed/Released 状态交给 ViewModel，由 ViewModel 维护持续移动状态。
        emit commandGenerated(GameCommand::input_command(action, ButtonState::Pressed));
    } else if (action == InputAction::LightAttack ||
               action == InputAction::HeavyAttack ||
               action == InputAction::Jump         ||
               action == InputAction::Restart      ||
               action == InputAction::Confirm      ||
               action == InputAction::Pause) {
        // 单次触发动作
        if (!m_triggeredThisPress.contains(key)) {
            m_triggeredThisPress.insert(key);
            emit commandGenerated(GameCommand::input_command(action, ButtonState::Triggered));
        }
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    const int key = event->key();
    if (!isHandledActionKey(key)) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    m_triggeredThisPress.remove(key);

    const InputAction action = keyToAction(key, false);

    if (isMovementAction(action)) {
        emit commandGenerated(GameCommand::input_command(action, ButtonState::Released));
    }
}

// ============================================================================
// 窗口大小变化
// ============================================================================

void GameWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const float vpW = m_snapshot.map.viewportWidth > 0.0f
                          ? m_snapshot.map.viewportWidth : 960.0f;
    const float vpH = m_snapshot.map.viewportHeight > 0.0f
                          ? m_snapshot.map.viewportHeight : 540.0f;

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
    const float vpW = m_snapshot.map.viewportWidth > 0.0f
                          ? m_snapshot.map.viewportWidth : 960.0f;
    const float vpH = m_snapshot.map.viewportHeight > 0.0f
                          ? m_snapshot.map.viewportHeight : 540.0f;
    m_scaleX = static_cast<float>(width()) / vpW;
    m_scaleY = static_cast<float>(height()) / vpH;

    const GamePhase phase = m_snapshot.phase;

    // ---- 标题画面 ----
    if (phase == GamePhase::Title) {
        drawOverlay(p);
        return;
    }

    // ---- 游戏画面 ----
    drawBackground(p);
    drawStreet(p);

    // 按 depthSortY 排序绘制所有角色（远的先画）
    std::vector<const ActorSnapshot*> drawList;

    // 玩家
    if (m_snapshot.player.visible) {
        drawList.push_back(&m_snapshot.player);
    }

    // 敌人
    for (const auto& enemy : m_snapshot.enemies) {
        if (enemy.visible) {
            drawList.push_back(&enemy);
        }
    }

    // 特效
    for (const auto& effect : m_snapshot.effects) {
        if (effect.visible) {
            drawList.push_back(&effect);
        }
    }

    // 按 depthSortY 排序（laneY + z 作为排序依据，远的先画）
    std::sort(drawList.begin(), drawList.end(),
              [](const ActorSnapshot* a, const ActorSnapshot* b) {
                  const float ya = a->position.laneY + a->position.z;
                  const float yb = b->position.laneY + b->position.z;
                  return ya < yb;
              });

    for (const auto* actor : drawList) {
        drawActor(p, *actor);
    }

    // ---- HUD ----
    drawHUD(p);

    // ---- GO 指示器 ----
    if (m_snapshot.map.showGoIndicator) {
        drawGOIndicator(p);
    }

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
    const float vpW = m_snapshot.map.viewportWidth > 0.0f
                          ? m_snapshot.map.viewportWidth : 960.0f;
    const float vpH = m_snapshot.map.viewportHeight > 0.0f
                          ? m_snapshot.map.viewportHeight : 540.0f;
    const float streetTop = m_snapshot.map.streetTopY > 0.0f
                                ? m_snapshot.map.streetTopY : 300.0f;

    // 天空渐变
    QLinearGradient skyGrad(0, 0, 0, streetTop * m_scaleY);
    skyGrad.setColorAt(0.0, QColor(30, 40, 80));     // 深蓝夜空
    skyGrad.setColorAt(0.6, QColor(60, 80, 140));
    skyGrad.setColorAt(1.0, QColor(120, 150, 200));  // 地平线亮色
    p.fillRect(QRectF(0, 0, vpW * m_scaleX, streetTop * m_scaleY), skyGrad);

    // 远景建筑
    drawBuildings(p);
}

void GameWidget::drawBuildings(QPainter& p)
{
    const float streetTop = m_snapshot.map.streetTopY > 0.0f
                                ? m_snapshot.map.streetTopY : 300.0f;
    const float camX = m_snapshot.map.cameraX;

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
    const float vpW = m_snapshot.map.viewportWidth > 0.0f
                          ? m_snapshot.map.viewportWidth : 960.0f;
    const float vpH = m_snapshot.map.viewportHeight > 0.0f
                          ? m_snapshot.map.viewportHeight : 540.0f;
    const float streetTop = m_snapshot.map.streetTopY > 0.0f
                                ? m_snapshot.map.streetTopY : 300.0f;
    const float streetBottom = m_snapshot.map.streetBottomY > 0.0f
                                   ? m_snapshot.map.streetBottomY : 500.0f;
    const float camX = m_snapshot.map.cameraX;

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
        return QColor(50, 100, 220);   // 蓝色 — 玩家
    case Team::Enemy:
        if (kind == ActorKind::Boss) {
            return QColor(180, 40, 40); // 深红 — Boss
        }
        return QColor(200, 70, 50);    // 红 — 普通小怪
    default:
        return QColor(150, 150, 150);  // 灰 — 中立/道具/特效
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

void GameWidget::drawActor(QPainter& p, const ActorSnapshot& actor)
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

    // 无敌闪烁（每 0.1 秒切换可见性）
    if (actor.invincible) {
        const int tick = static_cast<int>(m_snapshot.elapsedSeconds * 10.0f);
        if (tick % 2 == 0) return;
    }

    const QColor bodyColor = actorBodyColor(actor.team, actor.kind);

    p.save();
    p.translate(cx, cy);

    // 根据 facing 翻转
    if (actor.facing == Facing::Left) {
        p.scale(-1.0f, 1.0f);
    }

    // 根据状态绘制不同姿势
    drawCharacterBody(p, actor, bodyColor);

    p.restore();

    // 血条（敌人和 Boss 头顶）
    if (actor.team == Team::Enemy && actor.health.maximum > 0) {
        drawHealthBar(p, actor);
    }

}

void GameWidget::drawCharacterBody(QPainter& p, const ActorSnapshot& actor,
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

    const ActorState state = actor.state;

    // ---- 阴影（地面上椭圆） ----
    p.setPen(Qt::NoPen);
    p.fillRect(QRectF(-w * 0.35f, h - 4.0f * m_scaleY, w * 0.7f, 4.0f * m_scaleY),
               QColor(0, 0, 0, 60));

    // ---- 腿 ----
    if (state == ActorState::Jump || state == ActorState::AirAttack) {
        // 跳跃中腿收起
        p.fillRect(QRectF(-legW * 0.5f, legTop, legW, legH * 0.5f), pantsColor);
        p.fillRect(QRectF(legW * 0.3f, legTop, legW, legH * 0.5f), pantsColor);
    } else if (state == ActorState::Walk || state == ActorState::Run) {
        // 行走时前后腿
        const float stride = std::sin(m_snapshot.elapsedSeconds * 10.0f) * 6.0f * m_scaleX;
        p.fillRect(QRectF(-legW - stride, legTop, legW, legH), pantsColor);
        p.fillRect(QRectF(stride, legTop, legW, legH), pantsColor);
        // 鞋子
        p.fillRect(QRectF(-legW - stride - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
                          legW + 4.0f * m_scaleX, 5.0f * m_scaleY), shoeColor);
        p.fillRect(QRectF(stride - 2.0f * m_scaleX, legTop + legH - 5.0f * m_scaleY,
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
    QColor drawBodyColor = bodyColor;
    if (state == ActorState::Hurt) {
        drawBodyColor = QColor(255, 100, 100); // 受击闪红
    }

    p.fillRect(QRectF(-bodyW / 2, bodyTop, bodyW, bodyH), drawBodyColor);

    // 衣服纹理线
    p.setPen(QPen(darkerBody, 1.0f));
    p.drawLine(QPointF(-bodyW * 0.3f, bodyTop + bodyH * 0.3f),
               QPointF(bodyW * 0.3f, bodyTop + bodyH * 0.3f));

    // ---- 手臂 ----
    if (state == ActorState::LightAttack || state == ActorState::HeavyAttack ||
        state == ActorState::ComboFinisher) {
        // 攻击时手臂伸出（向 facing 方向 = 向右）
        const float extend = (state == ActorState::HeavyAttack) ? armW * 3.0f : armW * 2.0f;
        p.fillRect(QRectF(bodyW * 0.2f, bodyTop + bodyH * 0.15f, extend, armH * 0.5f),
                   skinColor);
        // 拳头
        p.fillRect(QRectF(bodyW * 0.2f + extend - 4.0f * m_scaleX,
                          bodyTop + bodyH * 0.15f - 3.0f * m_scaleY,
                          8.0f * m_scaleX, armH * 0.5f + 6.0f * m_scaleY),
                   skinColor.darker(120));
        // 另一只手稍后
        p.fillRect(QRectF(-bodyW * 0.5f, bodyTop + bodyH * 0.3f, armW, armH * 0.45f), skinColor);
    } else if (state == ActorState::AirAttack) {
        // 飞踢：腿伸出
        p.fillRect(QRectF(bodyW * 0.1f, legTop, legW * 2.5f, legH * 0.4f), pantsColor);
        p.fillRect(QRectF(-bodyW * 0.5f, bodyTop + bodyH * 0.25f, armW, armH * 0.4f), skinColor);
    } else {
        // 正常手臂
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
    if (state == ActorState::Dead) {
        p.setPen(QPen(QColor(255, 60, 60, 120), 2.0f));
        p.drawLine(QPointF(-w * 0.4f, h * 0.1f), QPointF(w * 0.4f, h * 0.5f));
        p.drawLine(QPointF(w * 0.4f, h * 0.1f), QPointF(-w * 0.4f, h * 0.5f));
    }

    if (state == ActorState::Hurt) {
        // 受击星效
        p.setPen(QPen(QColor(255, 200, 50, 180), 2.0f));
        const float sx = w * 0.4f;
        const float sy = h * 0.15f;
        p.drawLine(QPointF(sx - 6, sy), QPointF(sx + 6, sy));
        p.drawLine(QPointF(sx, sy - 6), QPointF(sx, sy + 6));
        p.drawLine(QPointF(sx - 4, sy - 4), QPointF(sx + 4, sy + 4));
        p.drawLine(QPointF(sx - 4, sy + 4), QPointF(sx + 4, sy - 4));
    }
}

void GameWidget::drawHealthBar(QPainter& p, const ActorSnapshot& actor)
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
    const auto& hud = m_snapshot.hud;
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
    drawBar(p, barStartX, hpY, barW, barH, hud.playerHealth.ratio(),
            healthBarColor(hud.playerHealth.ratio()),
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

    // ---- 连招计数 ----
    if (hud.comboStep > 0) {
        const float comboX = (m_snapshot.map.viewportWidth * m_scaleX) / 2.0f;
        const float comboY = margin * m_scaleY + 30.0f * m_scaleY;

        QFont comboFont("Arial", 16, QFont::Bold);
        p.setFont(comboFont);

        // 根据 comboStep 变色
        QColor comboColor;
        switch (hud.comboStep) {
        case 1: comboColor = QColor(255, 255, 255); break;
        case 2: comboColor = QColor(255, 220, 60);  break;
        case 3: comboColor = QColor(255, 80,  40);  break;
        default: comboColor = QColor(255, 255, 255); break;
        }

        p.setPen(comboColor);
        const QString comboText = QString("%1 HIT").arg(hud.comboStep);
        p.drawText(QRectF(comboX - 60.0f * m_scaleX, comboY,
                          120.0f * m_scaleX, 24.0f * m_scaleY),
                   Qt::AlignCenter, comboText);
    }

    // ---- Boss 血条 (上方居中偏右) ----
    if (hud.showBossHealth) {
        const float bossBarW = 250.0f * m_scaleX;
        const float bossX = (m_snapshot.map.viewportWidth * m_scaleX) / 2.0f - bossBarW / 2;
        const float bossY = margin * m_scaleY;

        p.setPen(QColor(255, 200, 100));
        QFont bossFont("Arial", 11, QFont::Bold);
        p.setFont(bossFont);
        p.drawText(QPointF(bossX, bossY + barH * 0.8f), "BOSS");

        drawBar(p, bossX + 45.0f * m_scaleX, bossY, bossBarW, barH,
                hud.bossHealth.ratio(),
                QColor(200, 40, 40),
                QString("%1 / %2").arg(hud.bossHealth.current).arg(hud.bossHealth.maximum));
    }

    // ---- 屏幕消息 ----
    if (!m_snapshot.screenMessage.empty()) {
        const float msgY = m_snapshot.map.viewportHeight * m_scaleY * 0.5f;
        const float msgX = m_snapshot.map.viewportWidth * m_scaleX * 0.5f;

        QFont msgFont("Arial", 18, QFont::Bold);
        p.setFont(msgFont);
        p.setPen(QColor(255, 255, 100));
        p.drawText(QRectF(msgX - 150.0f * m_scaleX, msgY - 20.0f * m_scaleY,
                          300.0f * m_scaleX, 40.0f * m_scaleY),
                   Qt::AlignCenter, QString::fromStdString(m_snapshot.screenMessage));
    }

    // ---- 关卡进度条 (底部) ----
    if (m_snapshot.map.worldWidth > 0.0f) {
        const float progBarW = width() * 0.6f;
        const float progBarH = 6.0f * m_scaleY;
        const float progX = (width() - progBarW) / 2.0f;
        const float progY = height() - progBarH - 8.0f * m_scaleY;

        const float progress = m_snapshot.progress.progressRatio > 0.0f
                                   ? m_snapshot.progress.progressRatio
                                   : m_snapshot.player.position.x / m_snapshot.map.worldWidth;

        p.fillRect(QRectF(progX, progY, progBarW, progBarH), QColor(30, 30, 30, 180));
        p.fillRect(QRectF(progX, progY, progBarW * progress, progBarH),
                   QColor(255, 200, 50, 200));

        // 遭遇战标记
        for (const auto& enc : m_snapshot.map.encounters) {
            if (m_snapshot.map.worldWidth > 0.0f) {
                const float markerX = progX + (enc.triggerX / m_snapshot.map.worldWidth) * progBarW;
                QColor markerColor = (enc.state == EncounterState::Cleared)
                                         ? QColor(60, 200, 60)
                                         : QColor(220, 60, 60);
                p.fillRect(QRectF(markerX - 3.0f, progY - 4.0f, 6.0f, progBarH + 8.0f),
                           markerColor);
            }
        }

        p.setPen(QPen(QColor(150, 150, 150, 150), 1.0f));
        p.drawRect(QRectF(progX, progY, progBarW, progBarH));
    }
}

// ============================================================================
// GO 指示器
// ============================================================================

void GameWidget::drawGOIndicator(QPainter& p)
{
    // 闪烁：0.5 秒亮 / 0.5 秒灭
    const float blinkCycle = std::fmod(m_goBlinkTimer, 1.0f);
    if (blinkCycle > 0.5f) return;

    const float centerX = m_snapshot.map.viewportWidth * m_scaleX * 0.75f;
    const float centerY = m_snapshot.map.viewportHeight * m_scaleY * 0.4f;

    QFont goFont("Arial", 24, QFont::Bold);
    p.setFont(goFont);

    // 发光效果（多层描边模拟）
    QColor glowColors[] = {
        QColor(255, 255, 100, 50),
        QColor(255, 255, 100, 80),
        QColor(255, 255, 50, 200),
    };

    for (int i = 2; i >= 0; --i) {
        p.setPen(QPen(glowColors[i], static_cast<float>(3 + i * 3)));
        p.drawText(QRectF(centerX - 80.0f * m_scaleX, centerY - 30.0f * m_scaleY,
                          160.0f * m_scaleX, 60.0f * m_scaleY),
                   Qt::AlignCenter, QString::fromUtf8("GO ▶"));
    }

    p.setPen(QColor(255, 255, 255));
    p.drawText(QRectF(centerX - 80.0f * m_scaleX, centerY - 30.0f * m_scaleY,
                      160.0f * m_scaleX, 60.0f * m_scaleY),
               Qt::AlignCenter, QString::fromUtf8("GO ▶"));
}

// ============================================================================
// 覆盖层（标题 / 暂停 / 结算 / 胜利）
// ============================================================================

void GameWidget::drawOverlay(QPainter& p)
{
    const float vpW = m_snapshot.map.viewportWidth > 0.0f
                          ? m_snapshot.map.viewportWidth : 960.0f;
    const float vpH = m_snapshot.map.viewportHeight > 0.0f
                          ? m_snapshot.map.viewportHeight : 540.0f;

    // 半透明黑色遮罩
    p.fillRect(QRectF(0, 0, vpW * m_scaleX, vpH * m_scaleY),
               QColor(0, 0, 0, 180));

    const float cx = vpW * m_scaleX / 2.0f;
    const float cy = vpH * m_scaleY / 2.0f;

    const GamePhase phase = m_snapshot.phase;

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
        const auto& result = m_snapshot.result;
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
        const auto& result = m_snapshot.result;
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

} // namespace alleyfist
