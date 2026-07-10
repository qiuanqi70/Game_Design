#pragma once

#include "../common/snapshot.h"
#include "../common/actions.h"

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
    void emitMoveCommands();
    InputAction keyToAction(int qtKey, bool pressed) const;

    // ---- 绘制子过程 ----
    void drawBackground(QPainter& p);
    void drawStreet(QPainter& p);
    void drawBuildings(QPainter& p);
    void drawActor(QPainter& p, const ActorViewData& actor);
    void drawCharacterBody(QPainter& p, const ActorViewData& actor, QColor bodyColor);
    void drawHealthBar(QPainter& p, const ActorViewData& actor);
    void drawPlayerStatus(QPainter& p);
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
    QTimer* m_timer = nullptr;
    QElapsedTimer m_elapsed;
    std::uint64_t m_frameIndex = 0;

    // 跟踪当前按住的键，用于持续移动和防止重复 Triggered。
    QSet<int> m_pressedKeys;
    QSet<int> m_triggeredThisPress; // 本轮 keyPress 已触发的单次动作

    // GO 闪烁
    mutable float m_goBlinkTimer = 0.0f;

    // 视口缩放
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
};

} // namespace alleyfist
