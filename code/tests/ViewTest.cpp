#include "AssetCatalog.h"
#include "GameWidget.h"
#include "InputDefs.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace alleyfist {
namespace {

// 视觉测试使用两档固定分辨率，并限制像素比较区域以定位具体 UI 元素。
const QSize kFullSize{960, 540};
const QSize kHalfSize{480, 270};
const QRect kActorRegion{340, 260, 280, 190};
const QRect kObjectRegion{430, 340, 100, 110};
const QRect kCenterOverlayRegion{220, 80, 520, 380};
const QRect kResultBannerRegion{250, 100, 460, 150};
const QRect kTitleBannerRegion{190, 35, 580, 180};
const QRect kTitleButtonRegion{310, 375, 340, 85};
const QRect kWaveRegion{300, 490, 360, 45};

// 为 MainWindow 转发测试中的每条命令通道分配稳定计数下标。
enum class CommandChannel : std::size_t {
    Left,
    Right,
    Up,
    Down,
    Primary,
    Secondary,
    StateToggle,
    Reset,
    Confirm,
    Pause,
    Tick,
    Count
};

// 构造可选自动重复标记的键盘事件，直接发送给被测 QWidget。
void send_key_event(QWidget& target, QEvent::Type type, int key,
                    bool autoRepeat = false)
{
    QKeyEvent event(type, key, Qt::NoModifier, QString(), autoRepeat, 1);
    QCoreApplication::sendEvent(&target, &event);
}

// 模拟一次完整按键，适合只在按下边沿触发的动作命令。
void click_key(QWidget& target, int key)
{
    send_key_event(target, QEvent::KeyPress, key);
    send_key_event(target, QEvent::KeyRelease, key);
}

// 处理已投递的刷新事件，保证事件计数和离屏截图已经更新。
void drain_events()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QCoreApplication::processEvents();
}

// 将控件按指定尺寸渲染到内存图片，供像素级视觉断言使用。
QImage render_widget(GameWidget& widget, const QSize& size = kFullSize)
{
    widget.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

// 统计指定区域内两张图片的不同像素，判断目标元素是否被绘制或改变。
int changed_pixel_count(const QImage& lhs, const QImage& rhs,
                        const QRect& requestedRegion)
{
    const QRect region = requestedRegion.intersected(lhs.rect())
                             .intersected(rhs.rect());
    int changed = 0;
    for (int y = region.top(); y <= region.bottom(); ++y) {
        for (int x = region.left(); x <= region.right(); ++x) {
            if (lhs.pixel(x, y) != rhs.pixel(x, y)) ++changed;
        }
    }
    return changed;
}

// 创建指定类型、动作、阵营和素材变体的标准角色测试数据。
ActorState make_actor(std::uint32_t id, ActorKind kind,
                      ActorActionState action, Team team = Team::Enemy,
                      ActorVisualVariant variant = ActorVisualVariant::Default)
{
    ActorState actor;
    actor.id = id;
    actor.kind = kind;
    actor.visualVariant = variant;
    actor.team = team;
    actor.position = {480.0f, 420.0f, 0.0f};
    actor.drawSize = kind == ActorKind::Boss ? Size{88.0f, 128.0f}
                                             : Size{64.0f, 96.0f};
    actor.health = kind == ActorKind::Boss ? ResourceBar{100, 140}
                                           : ResourceBar{18, 20};
    actor.actionState = action;
    actor.visible = true;
    return actor;
}

// 创建 Playing 阶段的最小状态，默认隐藏玩家以获得干净渲染基线。
GameState make_base_state()
{
    GameState state;
    state.elapsedSeconds = 8.0f;
    state.phase = GamePhase::Playing;
    state.progressRatio = 0.4f;
    state.player = make_actor(1, ActorKind::Player, ActorActionState::Idle,
                              Team::Player);
    state.player.visible = false;
    state.hud.playerHealth = {80, 100};
    state.hud.playerEnergy = {70, 100};
    return state;
}

// 创建包含玩家、敌人、HUD 和结果数据的完整状态，供窗口和覆盖层测试使用。
GameState make_rich_state()
{
    GameState state = make_base_state();
    state.player.visible = true;
    state.player.position = {420.0f, 420.0f, 0.0f};
    state.enemies.push_back(make_actor(
        2, ActorKind::Patroller, ActorActionState::Walk));
    state.enemies.back().position.x = 610.0f;
    state.screenMessage = "READY";
    state.hud.showBossHealth = true;
    state.hud.bossHealth = {90, 140};
    state.result.elapsedSeconds = 52.4f;
    state.result.defeatedEnemies = 12;
    return state;
}

// 同时统计实际 Paint 和 UpdateRequest，验证通知或动画确实触发重绘。
class WidgetEventCounter final : public QObject {
public:
    int paintCount = 0;
    int updateRequestCount = 0;

    // 在触发待测行为前清零两类事件计数。
    void reset() noexcept
    {
        paintCount = 0;
        updateRequestCount = 0;
    }

protected:
    // 被动记录绘制与刷新请求，不改变 QWidget 的事件处理结果。
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Paint) ++paintCount;
        if (event->type() == QEvent::UpdateRequest) ++updateRequestCount;
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

class ViewTest final : public QObject {
    Q_OBJECT

private slots:
    // 测试夹具初始化：切到空临时目录并显式加载静态 View 库中的 qrc 资源。
    void initTestCase()
    {
        m_originalWorkingDirectory = QDir::currentPath();
        m_temporaryDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temporaryDirectory->isValid());
        QVERIFY(QDir::setCurrent(m_temporaryDirectory->path()));
        QVERIFY(QDir().mkpath(QStringLiteral("assets/sfx")));

        // AssetCatalog 的构造会初始化静态 View 库拥有的资源集合。
        m_assets = std::make_unique<view::AssetCatalog>();
        QVERIFY(!m_assets->stageTileset.isNull());
    }

    // 测试结束后释放素材并恢复原工作目录，避免影响其他测试程序。
    void cleanupTestCase()
    {
        m_assets.reset();
        QVERIFY(QDir::setCurrent(m_originalWorkingDirectory));
        m_temporaryDirectory.reset();
    }

    // 测试同一方向的 WASD/方向键别名，直到最后一个实体键释放才发送停止边沿。
    void inputStateTracksAliasesUntilTheLastPhysicalKeyIsReleased()
    {
        view::InputState input;
        using Direction = view::MovementDirection;

        QVERIFY(input.press_movement(Direction::Left, Qt::Key_A));
        QVERIFY(!input.press_movement(Direction::Left, Qt::Key_A));
        QVERIFY(!input.press_movement(Direction::Left, Qt::Key_Left));
        QVERIFY(!input.release_movement(Direction::Left, Qt::Key_A));
        QVERIFY(!input.release_movement(Direction::Left, Qt::Key_A));
        QVERIFY(input.release_movement(Direction::Left, Qt::Key_Left));
        QVERIFY(!input.release_movement(Direction::Left, Qt::Key_Left));

        const auto active = input.clear_movement();
        for (const bool directionActive : active) QVERIFY(!directionActive);
    }

    // 测试相反方向可同时保持，clear_movement 会原子返回并清除全部活动方向。
    void inputStateKeepsOppositeDirectionsIndependentAndClearsAtomically()
    {
        view::InputState input;
        using Direction = view::MovementDirection;

        QVERIFY(input.press_movement(Direction::Left, Qt::Key_A));
        QVERIFY(input.press_movement(Direction::Right, Qt::Key_D));
        QVERIFY(input.press_movement(Direction::Up, Qt::Key_W));

        const auto active = input.clear_movement();
        QVERIFY(active[static_cast<std::size_t>(Direction::Left)]);
        QVERIFY(active[static_cast<std::size_t>(Direction::Right)]);
        QVERIFY(active[static_cast<std::size_t>(Direction::Up)]);
        QVERIFY(!active[static_cast<std::size_t>(Direction::Down)]);

        QVERIFY(!input.release_movement(Direction::Left, Qt::Key_A));
        QVERIFY(input.press_movement(Direction::Left, Qt::Key_A));
    }

    // 测试非法方向被拒绝，动作长按不重复触发，释放或清理后可再次触发。
    void inputStateRejectsInvalidDirectionsAndDeduplicatesActions()
    {
        view::InputState input;
        using Direction = view::MovementDirection;

        QVERIFY(!input.press_movement(Direction::Count, Qt::Key_A));
        QVERIFY(!input.release_movement(Direction::Count, Qt::Key_A));
        const auto invalid = static_cast<Direction>(999);
        QVERIFY(!input.press_movement(invalid, Qt::Key_A));
        QVERIFY(!input.release_movement(invalid, Qt::Key_A));

        QVERIFY(input.press_action(Qt::Key_J));
        QVERIFY(!input.press_action(Qt::Key_J));
        input.release_action(Qt::Key_J);
        QVERIFY(input.press_action(Qt::Key_J));
        input.clear_actions();
        QVERIFY(input.press_action(Qt::Key_J));
    }

    // 测试 GameWidget 将别名键合并为一个方向状态，同时独立保留相反方向。
    void widgetPreservesAliasAndOppositeDirectionEdges()
    {
        GameWidget widget;
        widget.setRunning(false);
        std::vector<bool> left;
        std::vector<bool> right;
        widget.set_move_left_command([&](bool pressed) { left.push_back(pressed); });
        widget.set_move_right_command([&](bool pressed) { right.push_back(pressed); });

        send_key_event(widget, QEvent::KeyPress, Qt::Key_A);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_Left);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_D);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_A);

        QCOMPARE(left, std::vector<bool>({true}));
        QCOMPARE(right, std::vector<bool>({true}));

        send_key_event(widget, QEvent::KeyRelease, Qt::Key_Left);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_D);
        QCOMPARE(left, std::vector<bool>({true, false}));
        QCOMPARE(right, std::vector<bool>({true, false}));
    }

    // 测试 GameWidget 忽略系统自动重复和重复按下，只转发真实输入边沿。
    void widgetRejectsAutoRepeatAndDeduplicatesActions()
    {
        GameWidget widget;
        widget.setRunning(false);
        std::vector<bool> movement;
        int primaryCalls = 0;
        widget.set_move_left_command(
            [&](bool pressed) { movement.push_back(pressed); });
        widget.set_primary_action_command([&]() { ++primaryCalls; });

        send_key_event(widget, QEvent::KeyPress, Qt::Key_A, true);
        QVERIFY(movement.empty());
        send_key_event(widget, QEvent::KeyPress, Qt::Key_A);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_A);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_A, true);
        QCOMPARE(movement, std::vector<bool>({true}));
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_A);
        QCOMPARE(movement, std::vector<bool>({true, false}));

        send_key_event(widget, QEvent::KeyPress, Qt::Key_J, true);
        QCOMPARE(primaryCalls, 0);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_J);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_J);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_J, true);
        QCOMPARE(primaryCalls, 1);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_J);
        click_key(widget, Qt::Key_J);
        QCOMPARE(primaryCalls, 2);
    }

    // 测试失去焦点或隐藏控件时释放移动状态，并清空动作去重状态。
    void focusOutAndHideReleaseMovementAndActionState()
    {
        GameWidget focusWidget;
        focusWidget.setRunning(false);
        std::vector<bool> left;
        std::vector<bool> up;
        int primaryCalls = 0;
        focusWidget.set_move_left_command(
            [&](bool pressed) { left.push_back(pressed); });
        focusWidget.set_move_up_command(
            [&](bool pressed) { up.push_back(pressed); });
        focusWidget.set_primary_action_command([&]() { ++primaryCalls; });

        send_key_event(focusWidget, QEvent::KeyPress, Qt::Key_A);
        send_key_event(focusWidget, QEvent::KeyPress, Qt::Key_W);
        send_key_event(focusWidget, QEvent::KeyPress, Qt::Key_J);
        QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(&focusWidget, &focusOut);

        QCOMPARE(left, std::vector<bool>({true, false}));
        QCOMPARE(up, std::vector<bool>({true, false}));
        send_key_event(focusWidget, QEvent::KeyPress, Qt::Key_J);
        QCOMPARE(primaryCalls, 2);

        GameWidget hideWidget;
        std::vector<bool> right;
        std::vector<bool> down;
        int secondaryCalls = 0;
        hideWidget.set_move_right_command(
            [&](bool pressed) { right.push_back(pressed); });
        hideWidget.set_move_down_command(
            [&](bool pressed) { down.push_back(pressed); });
        hideWidget.set_secondary_action_command([&]() { ++secondaryCalls; });
        hideWidget.show();
        drain_events();
        hideWidget.setRunning(false);

        send_key_event(hideWidget, QEvent::KeyPress, Qt::Key_D);
        send_key_event(hideWidget, QEvent::KeyPress, Qt::Key_S);
        send_key_event(hideWidget, QEvent::KeyPress, Qt::Key_K);
        hideWidget.hide();
        drain_events();

        QCOMPARE(right, std::vector<bool>({true, false}));
        QCOMPARE(down, std::vector<bool>({true, false}));
        hideWidget.show();
        drain_events();
        hideWidget.setRunning(false);
        send_key_event(hideWidget, QEvent::KeyPress, Qt::Key_K);
        QCOMPARE(secondaryCalls, 2);
        hideWidget.hide();
    }

    // 测试 MainWindow 向 GameWidget 转发状态、全部命令、通知和计时器生命周期。
    void mainWindowForwardsPropertyEveryCommandAndNotification()
    {
        MainWindow window;
        GameWidget& widget = window.get_game_widget();
        widget.setRunning(false);

        QCOMPARE(window.centralWidget(), static_cast<QWidget*>(&widget));
        QCOMPARE(window.windowTitle(), QStringLiteral("Alley Fist"));
        QCOMPARE(window.minimumSize(), QSize(480, 270));

        GameState state = make_rich_state();
        window.set_game_state(&state);
        const QImage playing = render_widget(widget);
        state.phase = GamePhase::Paused;
        window.set_game_state(&state);
        const QImage paused = render_widget(widget);
        QVERIFY(changed_pixel_count(playing, paused, kCenterOverlayRegion) > 500);

        std::array<int, static_cast<std::size_t>(CommandChannel::Count)> calls{};
        window.set_tick_command([&](float dt, std::uint64_t frame) {
            QVERIFY(dt >= 0.0f && dt <= 0.1f);
            QVERIFY(frame > 0);
            ++calls[static_cast<std::size_t>(CommandChannel::Tick)];
        });
        window.set_move_left_command([&](bool) {
            ++calls[static_cast<std::size_t>(CommandChannel::Left)];
        });
        window.set_move_right_command([&](bool) {
            ++calls[static_cast<std::size_t>(CommandChannel::Right)];
        });
        window.set_move_up_command([&](bool) {
            ++calls[static_cast<std::size_t>(CommandChannel::Up)];
        });
        window.set_move_down_command([&](bool) {
            ++calls[static_cast<std::size_t>(CommandChannel::Down)];
        });
        window.set_primary_action_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::Primary)];
        });
        window.set_secondary_action_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::Secondary)];
        });
        window.set_state_toggle_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::StateToggle)];
        });
        window.set_reset_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::Reset)];
        });
        window.set_confirm_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::Confirm)];
        });
        window.set_pause_command([&]() {
            ++calls[static_cast<std::size_t>(CommandChannel::Pause)];
        });

        click_key(widget, Qt::Key_A);
        click_key(widget, Qt::Key_D);
        click_key(widget, Qt::Key_W);
        click_key(widget, Qt::Key_S);
        click_key(widget, Qt::Key_J);
        click_key(widget, Qt::Key_K);
        click_key(widget, Qt::Key_Space);
        click_key(widget, Qt::Key_R);
        click_key(widget, Qt::Key_Return);
        click_key(widget, Qt::Key_P);

        for (std::size_t i = static_cast<std::size_t>(CommandChannel::Left);
             i <= static_cast<std::size_t>(CommandChannel::Down); ++i) {
            QCOMPARE(calls[i], 2);
        }
        for (std::size_t i = static_cast<std::size_t>(CommandChannel::Primary);
             i <= static_cast<std::size_t>(CommandChannel::Pause); ++i) {
            QCOMPARE(calls[i], 1);
        }

        WidgetEventCounter counter;
        widget.installEventFilter(&counter);
        window.show();
        drain_events();
        widget.setRunning(false);
        drain_events();
        counter.reset();
        window.get_notification()(0x1234u);
        QTRY_VERIFY_WITH_TIMEOUT(counter.paintCount > 0 ||
                                     counter.updateRequestCount > 0,
                                 500);

        counter.reset();
        widget.setRunning(true);
        QTRY_VERIFY_WITH_TIMEOUT(
            calls[static_cast<std::size_t>(CommandChannel::Tick)] >= 2, 500);
        window.hide();
        drain_events();
        const int stoppedTicks =
            calls[static_cast<std::size_t>(CommandChannel::Tick)];
        QTest::qWait(50);
        QCOMPARE(calls[static_cast<std::size_t>(CommandChannel::Tick)],
                 stoppedTicks);
        widget.removeEventFilter(&counter);
        window.set_game_state(nullptr);
    }

    // 测试 GameWidget 仅在显示期间启动 tick，隐藏后停止且帧号单调递增。
    void timerLifecycleStartsOnShowAndStopsOnHide()
    {
        GameWidget widget;
        int ticks = 0;
        std::uint64_t previousFrame = 0;
        widget.set_tick_command([&](float dt, std::uint64_t frame) {
            QVERIFY(dt >= 0.0f && dt <= 0.1f);
            QVERIFY(frame > previousFrame);
            previousFrame = frame;
            ++ticks;
        });

        QTest::qWait(40);
        QCOMPARE(ticks, 0);
        widget.show();
        QTRY_VERIFY_WITH_TIMEOUT(ticks >= 2, 500);
        widget.hide();
        drain_events();
        const int stoppedTicks = ticks;
        QTest::qWait(50);
        QCOMPARE(ticks, stoppedTicks);
    }

    // 为持续重绘测试提供所有带动画或覆盖层的非 Playing 阶段。
    void overlayPhasesKeepRepainting_data()
    {
        QTest::addColumn<int>("phase");
        QTest::newRow("title") << static_cast<int>(GamePhase::Title);
        QTest::newRow("paused") << static_cast<int>(GamePhase::Paused);
        QTest::newRow("game-over") << static_cast<int>(GamePhase::GameOver);
        QTest::newRow("win") << static_cast<int>(GamePhase::Win);
    }

    // 测试标题、暂停、失败和胜利阶段在没有外部通知时仍持续刷新动画。
    void overlayPhasesKeepRepainting()
    {
        QFETCH(int, phase);
        GameState state = make_rich_state();
        state.phase = static_cast<GamePhase>(phase);
        GameWidget widget;
        widget.set_game_state(&state);
        WidgetEventCounter counter;
        widget.installEventFilter(&counter);
        widget.show();
        drain_events();
        counter.reset();
        QTRY_VERIFY_WITH_TIMEOUT(counter.paintCount >= 2, 500);
        widget.hide();
        widget.removeEventFilter(&counter);
    }

    // 枚举角色、敌人、Boss、场景、HUD、界面和投射物等关键 qrc 图片。
    void qrcImagesExistAndDecode_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("player")
            << QStringLiteral(":/art/Spritesheets/Brawler Girl/idle.png");
        QTest::newRow("enemy")
            << QStringLiteral(":/art/Spritesheets/Enemy Punk/walk.png");
        QTest::newRow("boss") << QStringLiteral(":/art/Boss/Attack1.png");
        QTest::newRow("stage")
            << QStringLiteral(":/art/Stage Layers/tileset.png");
        QTest::newRow("health-hud")
            << QStringLiteral(":/art/HUD/HealthBar1.png");
        QTest::newRow("interface")
            << QStringLiteral(":/art/界面图/1 Frames/Frame_01.png");
        QTest::newRow("projectile")
            << QStringLiteral(":/art/远程型敌人2（机器人）/Ball1.png");
    }

    // 测试每个关键 qrc 路径都存在、可读取，并能解码为有效 QPixmap。
    void qrcImagesExistAndDecode()
    {
        QFETCH(QString, path);
        QFile file(path);
        QVERIFY2(file.exists(), qPrintable(path));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(path));
        QVERIFY(file.size() > 0);

        const QPixmap pixmap(path);
        QVERIFY2(!pixmap.isNull(), qPrintable(path));
        QVERIFY(pixmap.width() > 0);
        QVERIFY(pixmap.height() > 0);
    }

    // 测试像素字体已被打包进 qrc，且资源内容非空。
    void qrcFontExists()
    {
        QFile font(QStringLiteral(
            ":/art/界面图/10 Font/CyberpunkCraftpixPixel.otf"));
        QVERIFY(font.exists());
        QVERIFY(font.open(QIODevice::ReadOnly));
        QVERIFY(font.size() > 0);
    }

    // 测试各游戏阶段只需修改 GameState，就会在预期屏幕区域绘制不同覆盖层。
    void phaseOverlaysChangeTheirExpectedRegions()
    {
        GameState state = make_rich_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage playing = render_widget(widget);

        state.phase = GamePhase::Title;
        const QImage title = render_widget(widget);
        QVERIFY(changed_pixel_count(playing, title, kTitleBannerRegion) > 500);
        QVERIFY(changed_pixel_count(playing, title, kTitleButtonRegion) > 100);

        state.phase = GamePhase::Paused;
        const QImage paused = render_widget(widget);
        QVERIFY(changed_pixel_count(playing, paused, kCenterOverlayRegion) > 500);

        state.phase = GamePhase::GameOver;
        const QImage gameOver = render_widget(widget);
        QVERIFY(changed_pixel_count(playing, gameOver, kResultBannerRegion) > 500);

        state.phase = GamePhase::Win;
        const QImage win = render_widget(widget);
        QVERIFY(changed_pixel_count(playing, win, kResultBannerRegion) > 500);
        QVERIFY(changed_pixel_count(gameOver, win, kResultBannerRegion) > 50);
    }

    // 为角色种类渲染测试提供玩家、四类普通敌人、远程变体和 Boss 数据行。
    void actorKindsRenderInsideTheActorRegion_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("action");
        QTest::addColumn<int>("team");
        QTest::addColumn<int>("variant");

        QTest::newRow("player")
            << static_cast<int>(ActorKind::Player)
            << static_cast<int>(ActorActionState::Idle)
            << static_cast<int>(Team::Player)
            << static_cast<int>(ActorVisualVariant::Default);
        QTest::newRow("patroller")
            << static_cast<int>(ActorKind::Patroller)
            << static_cast<int>(ActorActionState::Walk)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::Default);
        QTest::newRow("ambusher")
            << static_cast<int>(ActorKind::Ambusher)
            << static_cast<int>(ActorActionState::Ambush)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::Default);
        QTest::newRow("charger")
            << static_cast<int>(ActorKind::Charger)
            << static_cast<int>(ActorActionState::Charge)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::Default);
        QTest::newRow("ranged-gunner")
            << static_cast<int>(ActorKind::Ranged)
            << static_cast<int>(ActorActionState::RangedAttack)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::RangedGunner);
        QTest::newRow("ranged-robot")
            << static_cast<int>(ActorKind::Ranged)
            << static_cast<int>(ActorActionState::RangedAttack)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::RangedRobot);
        QTest::newRow("boss")
            << static_cast<int>(ActorKind::Boss)
            << static_cast<int>(ActorActionState::HeavyAttack)
            << static_cast<int>(Team::Enemy)
            << static_cast<int>(ActorVisualVariant::Default);
    }

    // 测试每种角色/素材变体都能在角色目标区域产生可见绘制结果。
    void actorKindsRenderInsideTheActorRegion()
    {
        QFETCH(int, kind);
        QFETCH(int, action);
        QFETCH(int, team);
        QFETCH(int, variant);

        GameState state = make_base_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage baseline = render_widget(widget);

        state.enemies.push_back(make_actor(
            10, static_cast<ActorKind>(kind),
            static_cast<ActorActionState>(action), static_cast<Team>(team),
            static_cast<ActorVisualVariant>(variant)));
        const QImage actor = render_widget(widget);
        QVERIFY2(changed_pixel_count(baseline, actor, kActorRegion) > 20,
                 "actor kind did not alter its target region");
    }

    // 枚举 ActorActionState 的全部动作，确保数据驱动测试没有遗漏分支。
    void actorActionsRenderInsideTheActorRegion_data()
    {
        QTest::addColumn<int>("action");
        const std::array<std::pair<const char*, ActorActionState>, 11> actions = {{
            {"idle", ActorActionState::Idle},
            {"walk", ActorActionState::Walk},
            {"light", ActorActionState::LightAttack},
            {"heavy", ActorActionState::HeavyAttack},
            {"ranged", ActorActionState::RangedAttack},
            {"ambush", ActorActionState::Ambush},
            {"charge", ActorActionState::Charge},
            {"jump", ActorActionState::Jump},
            {"air", ActorActionState::AirAttack},
            {"hurt", ActorActionState::Hurt},
            {"dead", ActorActionState::Dead},
        }};
        for (const auto& [name, action] : actions) {
            QTest::newRow(name) << static_cast<int>(action);
        }
    }

    // 测试每种角色动作（含受击和死亡）都能在角色区域被渲染。
    void actorActionsRenderInsideTheActorRegion()
    {
        QFETCH(int, action);
        GameState state = make_base_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage baseline = render_widget(widget);

        state.enemies.push_back(make_actor(
            20, ActorKind::Boss, static_cast<ActorActionState>(action)));
        if (static_cast<ActorActionState>(action) == ActorActionState::Hurt) {
            state.enemies.back().lastImpact = ImpactLevel::Heavy;
            state.enemies.back().impactRevision = 1;
        }
        const QImage actionImage = render_widget(widget);
        QVERIFY2(changed_pixel_count(baseline, actionImage, kActorRegion) > 20,
                 "actor action did not render in its target region");
    }

    // 测试投射物可见、阵营改变颜色，并为机器人远程敌人选用专属素材。
    void projectileKindAndTeamAffectTheTargetRegion()
    {
        GameState state = make_base_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage baseline = render_widget(widget);

        ProjectileState projectile;
        projectile.id = 30;
        projectile.kind = ProjectileKind::ThrownObject;
        projectile.position = {480.0f, 400.0f, 20.0f};
        projectile.facing = Facing::Right;
        projectile.team = Team::Enemy;
        state.projectiles = {projectile};
        const QImage enemy = render_widget(widget);
        QVERIFY(changed_pixel_count(baseline, enemy, kObjectRegion) > 4);

        state.projectiles.front().team = Team::Player;
        const QImage player = render_widget(widget);
        QVERIFY(changed_pixel_count(enemy, player, kObjectRegion) > 4);

        state.projectiles.front().team = Team::Enemy;
        state.projectiles.front().visualVariant = ActorVisualVariant::RangedRobot;
        const QImage robot = render_widget(widget);
        QVERIFY(changed_pixel_count(enemy, robot, kObjectRegion) > 4);
    }

    // 测试生命和能量补给都可见，并具有可区分的渲染结果。
    void pickupKindsAffectTheTargetRegion()
    {
        GameState state = make_base_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage baseline = render_widget(widget);

        PickupState pickup;
        pickup.id = 40;
        pickup.position = {480.0f, 420.0f, 0.0f};
        pickup.kind = PickupKind::Health;
        state.pickups = {pickup};
        const QImage health = render_widget(widget);
        QVERIFY(changed_pixel_count(baseline, health, kObjectRegion) > 10);

        state.pickups.front().kind = PickupKind::Energy;
        const QImage energy = render_widget(widget);
        QVERIFY(changed_pixel_count(health, energy, kObjectRegion) > 10);
    }

    // 测试波次编号/总数会更新 HUD，Boss Intro 进度会改变中央演出。
    void encounterUsesWaveNumbersAndBossIntroProgress()
    {
        GameState state = make_base_state();
        state.encounter.kind = EncounterKind::EnemyWave;
        state.encounter.phase = EncounterPhase::Fighting;
        state.encounter.currentWave = 1;
        state.encounter.totalWaves = 3;
        state.encounter.remainingEnemies = 4;
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage waveOne = render_widget(widget);

        state.encounter.currentWave = 2;
        const QImage waveTwo = render_widget(widget);
        QVERIFY(changed_pixel_count(waveOne, waveTwo, kWaveRegion) > 2);

        state.encounter.totalWaves = 4;
        const QImage fourWaves = render_widget(widget);
        QVERIFY(changed_pixel_count(waveTwo, fourWaves, kWaveRegion) > 2);

        state.encounter.kind = EncounterKind::Boss;
        state.encounter.phase = EncounterPhase::Intro;
        state.encounter.introProgress = 0.0f;
        const QImage warning = render_widget(widget);
        state.encounter.introProgress = 1.0f;
        const QImage revealed = render_widget(widget);
        QVERIFY(changed_pixel_count(warning, revealed,
                                    kCenterOverlayRegion) > 500);
    }

    // 测试缺少对应精灵图的角色动作会退回程序化绘制，而不是完全不可见。
    void missingActorArtUsesTheProceduralFallback()
    {
        ActorState unsupported = make_actor(
            50, ActorKind::Player, ActorActionState::RangedAttack,
            Team::Player);
        const auto* clip = m_assets->actor_clip(unsupported);
        QVERIFY(clip != nullptr);
        QVERIFY(clip->sheet.isNull());

        GameState state = make_base_state();
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);
        const QImage baseline = render_widget(widget);
        state.enemies = {unsupported};
        const QImage fallback = render_widget(widget);
        QVERIFY(changed_pixel_count(baseline, fallback, kActorRegion) > 20);
    }

    // 测试非法逻辑视口、异常资源条和越界进度值不会破坏全尺寸/半尺寸布局。
    void invalidViewportFallsBackAndKeepsCriticalLayoutStable()
    {
        GameState state = make_base_state();
        state.map.viewportWidth = 0.0f;
        state.map.viewportHeight = -1.0f;
        state.hud.playerHealth = {50, 0};
        state.hud.playerEnergy = {50, -1};
        state.hud.showBossHealth = true;
        state.hud.bossHealth = {140, 140};
        state.progressRatio = 0.0f;
        GameWidget widget;
        widget.setRunning(false);
        widget.set_game_state(&state);

        const QImage emptyProgress = render_widget(widget, kFullSize);
        state.progressRatio = 5.0f;
        const QImage clampedProgress = render_widget(widget, kFullSize);
        QVERIFY(emptyProgress.pixelColor(480, 528) !=
                clampedProgress.pixelColor(480, 528));

        state.progressRatio = 0.5f;
        state.phase = GamePhase::Playing;
        const QImage fullPlaying = render_widget(widget, kFullSize);
        state.phase = GamePhase::Paused;
        const QImage fullPaused = render_widget(widget, kFullSize);
        QVERIFY(changed_pixel_count(fullPlaying, fullPaused,
                                    kCenterOverlayRegion) > 500);

        state.phase = GamePhase::Playing;
        const QImage halfPlaying = render_widget(widget, kHalfSize);
        state.phase = GamePhase::Paused;
        const QImage halfPaused = render_widget(widget, kHalfSize);
        const QRect halfOverlay{110, 40, 260, 190};
        QVERIFY(changed_pixel_count(halfPlaying, halfPaused,
                                    halfOverlay) > 100);
        QCOMPARE(fullPaused.size(), kFullSize);
        QCOMPARE(halfPaused.size(), kHalfSize);
    }

private:
    QString m_originalWorkingDirectory;
    std::unique_ptr<QTemporaryDir> m_temporaryDirectory;
    std::unique_ptr<view::AssetCatalog> m_assets;
};

} // namespace alleyfist

int main(int argc, char** argv)
{
    // 离屏平台让 QWidget 渲染测试可以在没有桌面窗口系统的环境中执行。
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QApplication application(argc, argv);
    alleyfist::ViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ViewTest.moc"
