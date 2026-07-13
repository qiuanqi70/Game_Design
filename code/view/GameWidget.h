#pragma once

#include "../common/snapshot.h"
#include "../common/actions.h"

#include "InputDefs.h"

#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QWidget>

namespace alleyfist {

/// @brief 游戏渲染画布，负责键盘输入、定时器驱动和所有画面绘制。
///
/// GameWidget 只依赖 Common 层的 GameSnapshot 和 GameCommand，
/// 不直接操作游戏数据。
/// 它把 Qt 键盘事件翻译成逻辑命令，把只读快照翻译成像素绘制。
///
/// 键盘映射和 MovementIntent 聚合属于 View 层内部实现，
/// 换输入设备（手柄 / 触屏）只需改本文件的按键处理。
class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    /// 外部（App 层）调用，传入最新帧的快照并触发重绘。
    void updateSnapshot(const GameSnapshot& snapshot);

    /// 设置游戏循环是否运行。
    void setRunning(bool running);

signals:
    /// 键盘输入转换为 GameCommand 后发出。
    void commandGenerated(const GameCommand& command);

    /// 每帧定时器触发时发出，携带 delta 时间和帧序号。
    void tickRequested(float deltaSeconds, std::uint64_t frameIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // ---- 输入处理 ----

    /// 根据当前按住的移动键发送 GameCommand。
    void emitMovement();

    /// 判断某个 Qt 按键是否被 View 处理（而非交给基类）。
    static bool isHandledKey(int qtKey);

    // ---- 绘制子过程 ----
    void drawBackground(QPainter& p);
    void drawStreet(QPainter& p);
    void drawBuildings(QPainter& p);
    void drawActor(QPainter& p, const ActorSnapshot& actor);
    void drawCharacterBody(QPainter& p, const ActorSnapshot& actor,
                           QColor bodyColor);
    void drawHealthBar(QPainter& p, const ActorSnapshot& actor);
    void drawHUD(QPainter& p);
    void drawGOIndicator(QPainter& p);
    void drawOverlay(QPainter& p);

    // ---- 坐标转换 ----
    float worldToScreenX(float worldX) const;
    float worldToScreenY(float laneY, float z) const;

    // ---- 辅助绘制 ----
    static QColor actorBodyColor(Team team, ActorKind kind);
    static QColor healthBarColor(float ratio);
    static QColor energyBarColor(float ratio);
    void drawBar(QPainter& p, float x, float y, float w, float h,
                 float ratio, QColor fillColor, const QString& label);

    // ---- 状态 ----
    GameSnapshot m_snapshot;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    std::uint64_t m_frameIndex = 0;

    /// 当前按住的移动键状态，用于聚合 Direction。
    view::MovementIntent m_movement;

    /// 跟踪本轮按下已经触发过的单次动作，防止重复 Triggered。
    QSet<int> m_triggeredThisPress;

    // GO 闪烁
    mutable float m_goBlinkTimer = 0.0f;

    // 视口缩放
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
};

} // namespace alleyfist
