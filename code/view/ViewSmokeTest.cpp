#include "GameWidget.h"
#include "InputDefs.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace alleyfist {
namespace {

enum class CommandChannel {
    Primary,
    Secondary,
    StateToggle,
    Reset,
    Confirm,
    Pause
};

void send_key_event(QWidget& target, QEvent::Type type, int key,
                    bool autoRepeat = false)
{
    QKeyEvent event(type, key, Qt::NoModifier, QString(), autoRepeat, 1);
    QCoreApplication::sendEvent(&target, &event);
}

void click_key(QWidget& target, int key)
{
    send_key_event(target, QEvent::KeyPress, key);
    send_key_event(target, QEvent::KeyRelease, key);
}

QImage render_widget(GameWidget& widget, const QSize& size = {640, 360})
{
    widget.resize(size);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

bool contains_more_than_one_color(const QImage& image)
{
    if (image.isNull()) return false;

    const QRgb first = image.pixel(0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) != first) return true;
        }
    }
    return false;
}

ActorState make_actor(std::uint32_t id, ActorKind kind,
                      ActorActionState action, float x, float laneY)
{
    ActorState actor;
    actor.id = id;
    actor.kind = kind;
    actor.team = kind == ActorKind::Player ? Team::Player : Team::Enemy;
    actor.position = {x, laneY, 0.0f};
    actor.drawSize = kind == ActorKind::Boss ? Size{88.0f, 128.0f}
                                             : Size{54.0f, 82.0f};
    actor.health = kind == ActorKind::Boss ? ResourceBar{90, 140}
                                           : ResourceBar{16, 20};
    actor.actionState = action;
    actor.visible = true;
    return actor;
}

GameState make_rich_game_state()
{
    GameState state;
    state.elapsedSeconds = 12.5f;
    state.phase = GamePhase::Playing;
    state.progressRatio = 0.62f;
    state.map.cameraX = 120.0f;
    state.player = make_actor(1, ActorKind::Player,
                              ActorActionState::Walk, 510.0f, 430.0f);
    state.player.health = {73, 100};
    state.player.lastImpact = ImpactLevel::Light;
    state.player.impactRevision = 2;

    state.enemies = {
        make_actor(2, ActorKind::Patroller, ActorActionState::Idle,
                   630.0f, 350.0f),
        make_actor(3, ActorKind::Ambusher, ActorActionState::Ambush,
                   700.0f, 380.0f),
        make_actor(4, ActorKind::Charger, ActorActionState::Charge,
                   760.0f, 410.0f),
        make_actor(5, ActorKind::Ranged, ActorActionState::RangedAttack,
                   820.0f, 445.0f),
        make_actor(6, ActorKind::Ranged, ActorActionState::Hurt,
                   875.0f, 465.0f),
        make_actor(7, ActorKind::Boss, ActorActionState::HeavyAttack,
                   930.0f, 405.0f),
    };
    state.enemies[3].visualVariant = ActorVisualVariant::RangedGunner;
    state.enemies[4].visualVariant = ActorVisualVariant::RangedRobot;
    state.enemies[4].lastImpact = ImpactLevel::Heavy;
    state.enemies[4].impactRevision = 3;
    state.enemies[5].facing = Facing::Left;

    ProjectileState gunnerProjectile;
    gunnerProjectile.id = 10;
    gunnerProjectile.visualVariant = ActorVisualVariant::RangedGunner;
    gunnerProjectile.position = {690.0f, 390.0f, 24.0f};
    gunnerProjectile.facing = Facing::Left;
    state.projectiles.push_back(gunnerProjectile);

    ProjectileState robotProjectile;
    robotProjectile.id = 11;
    robotProjectile.visualVariant = ActorVisualVariant::RangedRobot;
    robotProjectile.position = {790.0f, 420.0f, 30.0f};
    robotProjectile.facing = Facing::Right;
    state.projectiles.push_back(robotProjectile);

    PickupState healthPickup;
    healthPickup.id = 20;
    healthPickup.kind = PickupKind::Health;
    healthPickup.position = {580.0f, 455.0f, 0.0f};
    state.pickups.push_back(healthPickup);

    PickupState energyPickup;
    energyPickup.id = 21;
    energyPickup.kind = PickupKind::Energy;
    energyPickup.position = {650.0f, 470.0f, 0.0f};
    state.pickups.push_back(energyPickup);

    state.hud.playerHealth = {73, 100};
    state.hud.playerEnergy = {18, 100};
    state.hud.playerExhausted = true;
    state.hud.showBossHealth = true;
    state.hud.bossHealth = {32, 140};
    state.encounter.kind = EncounterKind::EnemyWave;
    state.encounter.phase = EncounterPhase::Fighting;
    state.encounter.currentWave = 2;
    state.encounter.totalWaves = 3;
    state.encounter.remainingEnemies = 6;
    return state;
}

class UpdateRequestCounter final : public QObject {
public:
    int count = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::UpdateRequest) ++count;
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

class ViewSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_originalWorkingDirectory = QDir::currentPath();
        m_temporaryDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temporaryDirectory->isValid());
        QVERIFY(QDir::setCurrent(m_temporaryDirectory->path()));
        QVERIFY(QDir().mkpath(QStringLiteral("assets/sfx")));
    }

    void cleanupTestCase()
    {
        QVERIFY(QDir::setCurrent(m_originalWorkingDirectory));
        m_temporaryDirectory.reset();
    }

    void movementIntentTracksAndClearsDirections()
    {
        view::MovementIntent movement;
        QVERIFY(!movement.is_moving());

        movement.left = true;
        movement.up = true;
        QVERIFY(movement.is_moving());
        QVERIFY(movement.left);
        QVERIFY(movement.up);
        QVERIFY(!movement.right);
        QVERIFY(!movement.down);

        movement.clear();
        QVERIFY(!movement.is_moving());
        QVERIFY(!movement.left);
        QVERIFY(!movement.right);
        QVERIFY(!movement.up);
        QVERIFY(!movement.down);
    }

    void movementKeys_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("direction");

        QTest::newRow("left-arrow") << static_cast<int>(Qt::Key_Left) << 0;
        QTest::newRow("left-a") << static_cast<int>(Qt::Key_A) << 0;
        QTest::newRow("right-arrow") << static_cast<int>(Qt::Key_Right) << 1;
        QTest::newRow("right-d") << static_cast<int>(Qt::Key_D) << 1;
        QTest::newRow("up-arrow") << static_cast<int>(Qt::Key_Up) << 2;
        QTest::newRow("up-w") << static_cast<int>(Qt::Key_W) << 2;
        QTest::newRow("down-arrow") << static_cast<int>(Qt::Key_Down) << 3;
        QTest::newRow("down-s") << static_cast<int>(Qt::Key_S) << 3;
    }

    void movementKeys()
    {
        QFETCH(int, key);
        QFETCH(int, direction);

        GameWidget widget;
        widget.setRunning(false);
        std::array<std::vector<bool>, 4> calls;
        widget.set_move_left_command([&](bool value) { calls[0].push_back(value); });
        widget.set_move_right_command([&](bool value) { calls[1].push_back(value); });
        widget.set_move_up_command([&](bool value) { calls[2].push_back(value); });
        widget.set_move_down_command([&](bool value) { calls[3].push_back(value); });

        send_key_event(widget, QEvent::KeyPress, key);
        send_key_event(widget, QEvent::KeyPress, key);
        send_key_event(widget, QEvent::KeyPress, key, true);

        QCOMPARE(calls[direction].size(), std::size_t{1});
        QVERIFY(calls[direction][0]);
        for (int index = 0; index < 4; ++index) {
            if (index != direction) QVERIFY(calls[index].empty());
        }

        send_key_event(widget, QEvent::KeyRelease, key, true);
        QCOMPARE(calls[direction].size(), std::size_t{1});
        send_key_event(widget, QEvent::KeyRelease, key);
        QCOMPARE(calls[direction].size(), std::size_t{2});
        QVERIFY(!calls[direction][1]);
    }

    void oppositeMovementKeysRemainIndependent()
    {
        GameWidget widget;
        widget.setRunning(false);
        std::vector<bool> leftCalls;
        std::vector<bool> rightCalls;
        widget.set_move_left_command([&](bool value) { leftCalls.push_back(value); });
        widget.set_move_right_command([&](bool value) { rightCalls.push_back(value); });

        send_key_event(widget, QEvent::KeyPress, Qt::Key_A);
        send_key_event(widget, QEvent::KeyPress, Qt::Key_D);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_A);
        send_key_event(widget, QEvent::KeyRelease, Qt::Key_D);

        QCOMPARE(leftCalls.size(), std::size_t{2});
        QVERIFY(leftCalls[0]);
        QVERIFY(!leftCalls[1]);
        QCOMPARE(rightCalls.size(), std::size_t{2});
        QVERIFY(rightCalls[0]);
        QVERIFY(!rightCalls[1]);
    }

    void actionKeys_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("channel");

        QTest::newRow("primary-j") << static_cast<int>(Qt::Key_J)
                                    << static_cast<int>(CommandChannel::Primary);
        QTest::newRow("primary-z") << static_cast<int>(Qt::Key_Z)
                                    << static_cast<int>(CommandChannel::Primary);
        QTest::newRow("secondary-k") << static_cast<int>(Qt::Key_K)
                                      << static_cast<int>(CommandChannel::Secondary);
        QTest::newRow("secondary-x") << static_cast<int>(Qt::Key_X)
                                      << static_cast<int>(CommandChannel::Secondary);
        QTest::newRow("jump-space") << static_cast<int>(Qt::Key_Space)
                                     << static_cast<int>(CommandChannel::StateToggle);
        QTest::newRow("reset-r") << static_cast<int>(Qt::Key_R)
                                  << static_cast<int>(CommandChannel::Reset);
        QTest::newRow("confirm-return") << static_cast<int>(Qt::Key_Return)
                                         << static_cast<int>(CommandChannel::Confirm);
        QTest::newRow("confirm-enter") << static_cast<int>(Qt::Key_Enter)
                                        << static_cast<int>(CommandChannel::Confirm);
        QTest::newRow("pause-escape") << static_cast<int>(Qt::Key_Escape)
                                       << static_cast<int>(CommandChannel::Pause);
        QTest::newRow("pause-p") << static_cast<int>(Qt::Key_P)
                                  << static_cast<int>(CommandChannel::Pause);
    }

    void actionKeys()
    {
        QFETCH(int, key);
        QFETCH(int, channel);

        GameWidget widget;
        widget.setRunning(false);
        std::array<int, 6> calls{};
        widget.set_primary_action_command([&]() { ++calls[0]; });
        widget.set_secondary_action_command([&]() { ++calls[1]; });
        widget.set_state_toggle_command([&]() { ++calls[2]; });
        widget.set_reset_command([&]() { ++calls[3]; });
        widget.set_confirm_command([&]() { ++calls[4]; });
        widget.set_pause_command([&]() { ++calls[5]; });

        send_key_event(widget, QEvent::KeyPress, key);
        send_key_event(widget, QEvent::KeyPress, key);
        send_key_event(widget, QEvent::KeyPress, key, true);
        QCOMPARE(calls[channel], 1);

        send_key_event(widget, QEvent::KeyRelease, key, true);
        QCOMPARE(calls[channel], 1);
        send_key_event(widget, QEvent::KeyRelease, key);
        click_key(widget, key);
        QCOMPARE(calls[channel], 2);

        for (int index = 0; index < static_cast<int>(calls.size()); ++index) {
            if (index != channel) QCOMPARE(calls[index], 0);
        }
    }

    void unknownKeysAndMissingCallbacksAreSafe()
    {
        GameWidget widget;
        widget.setRunning(false);
        int callCount = 0;
        widget.set_primary_action_command([&]() { ++callCount; });
        widget.set_move_left_command([&](bool) { ++callCount; });

        click_key(widget, Qt::Key_Q);
        QCOMPARE(callCount, 0);

        GameWidget unboundWidget;
        unboundWidget.setRunning(false);
        const std::array<int, 10> handledKeys = {
            Qt::Key_A, Qt::Key_D, Qt::Key_W, Qt::Key_S, Qt::Key_J,
            Qt::Key_K, Qt::Key_Space, Qt::Key_R, Qt::Key_Return, Qt::Key_P,
        };
        for (const int key : handledKeys) click_key(unboundWidget, key);
        QVERIFY(true);
    }

    void mainWindowOwnsWidgetAndForwardsBindings()
    {
        GameState state;
        MainWindow window;
        GameWidget& widget = window.get_game_widget();
        widget.setRunning(false);

        QCOMPARE(window.windowTitle(), QStringLiteral("Alley Fist"));
        QCOMPARE(window.minimumSize(), QSize(480, 270));
        QCOMPARE(window.centralWidget(), static_cast<QWidget*>(&widget));
        QCOMPARE(widget.minimumSize(), QSize(320, 180));
        QCOMPARE(widget.focusPolicy(), Qt::StrongFocus);

        std::array<int, 11> calls{};
        window.set_tick_command([&](float, std::uint64_t) { ++calls[10]; });
        window.set_move_left_command([&](bool) { ++calls[0]; });
        window.set_move_right_command([&](bool) { ++calls[1]; });
        window.set_move_up_command([&](bool) { ++calls[2]; });
        window.set_move_down_command([&](bool) { ++calls[3]; });
        window.set_primary_action_command([&]() { ++calls[4]; });
        window.set_secondary_action_command([&]() { ++calls[5]; });
        window.set_state_toggle_command([&]() { ++calls[6]; });
        window.set_reset_command([&]() { ++calls[7]; });
        window.set_confirm_command([&]() { ++calls[8]; });
        window.set_pause_command([&]() { ++calls[9]; });

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

        QCOMPARE(calls[0], 2);
        QCOMPARE(calls[1], 2);
        QCOMPARE(calls[2], 2);
        QCOMPARE(calls[3], 2);
        for (int index = 4; index <= 9; ++index) {
            QCOMPARE(calls[index], 1);
        }

        widget.setRunning(true);
        QTest::qWait(100);
        widget.setRunning(false);
        QVERIFY(calls[10] > 0);

        state.phase = GamePhase::Paused;
        window.set_game_state(&state);
        const QImage paused = render_widget(widget);
        QVERIFY(contains_more_than_one_color(paused));

        UpdateRequestCounter updateCounter;
        window.installEventFilter(&updateCounter);
        window.show();
        QCoreApplication::processEvents();
        const int beforeNotification = updateCounter.count;
        window.get_notification()(42);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
        QCoreApplication::processEvents();
        QVERIFY(updateCounter.count > beforeNotification);
        window.removeEventFilter(&updateCounter);
        window.set_game_state(nullptr);
    }

    void timerCanBeStoppedAndRestarted()
    {
        GameWidget widget;
        widget.setRunning(false);
        int tickCount = 0;
        bool valuesAreValid = true;
        std::uint64_t previousFrame = 0;

        widget.set_tick_command([&](float dt, std::uint64_t frame) {
            valuesAreValid = valuesAreValid && dt >= 0.0f && dt <= 0.1f;
            valuesAreValid = valuesAreValid && frame > previousFrame;
            previousFrame = frame;
            ++tickCount;
        });

        QTest::qWait(40);
        QCOMPARE(tickCount, 0);

        widget.setRunning(true);
        QTRY_VERIFY_WITH_TIMEOUT(tickCount >= 2, 1000);
        widget.setRunning(false);
        QVERIFY(valuesAreValid);

        QCoreApplication::processEvents();
        const int stoppedCount = tickCount;
        QTest::qWait(40);
        QCOMPARE(tickCount, stoppedCount);
    }

    void rendersNullAndAllGamePhases()
    {
        GameState state = make_rich_game_state();
        GameWidget widget;
        widget.setRunning(false);

        widget.set_game_state(nullptr);
        const QImage nullState = render_widget(widget, {320, 180});
        QVERIFY(contains_more_than_one_color(nullState));

        std::vector<QImage> phaseImages;
        const std::array<GamePhase, 5> phases = {
            GamePhase::Title,
            GamePhase::Playing,
            GamePhase::Paused,
            GamePhase::GameOver,
            GamePhase::Win,
        };
        for (const GamePhase phase : phases) {
            state.phase = phase;
            state.result.elapsedSeconds = 123.4f;
            state.result.defeatedEnemies = 17;
            widget.set_game_state(&state);
            QImage image = render_widget(widget);
            QVERIFY(!image.isNull());
            QVERIFY(contains_more_than_one_color(image));
            phaseImages.push_back(std::move(image));
        }

        QVERIFY(phaseImages[0] != phaseImages[1]);
        QVERIFY(phaseImages[1] != phaseImages[2]);
        QVERIFY(phaseImages[3] != phaseImages[4]);
        widget.set_game_state(nullptr);
    }

    void rendersEncounterAndDefensiveStateVariants()
    {
        GameState state = make_rich_game_state();
        GameWidget widget;
        widget.setRunning(false);

        state.encounter.kind = EncounterKind::Boss;
        state.encounter.phase = EncounterPhase::Intro;
        for (const float progress : {0.0f, 0.38f, 0.75f, 1.0f, 2.0f}) {
            state.encounter.introProgress = progress;
            widget.set_game_state(&state);
            QVERIFY(contains_more_than_one_color(render_widget(widget)));
        }

        const std::array<ActorActionState, 11> actions = {
            ActorActionState::Idle,
            ActorActionState::Walk,
            ActorActionState::LightAttack,
            ActorActionState::HeavyAttack,
            ActorActionState::RangedAttack,
            ActorActionState::Ambush,
            ActorActionState::Charge,
            ActorActionState::Jump,
            ActorActionState::AirAttack,
            ActorActionState::Hurt,
            ActorActionState::Dead,
        };
        for (const ActorActionState action : actions) {
            state.player.actionState = action;
            state.player.visible = true;
            widget.set_game_state(&state);
            QVERIFY(contains_more_than_one_color(render_widget(widget, {480, 270})));
        }

        state.map.viewportWidth = 0.0f;
        state.map.viewportHeight = -1.0f;
        state.hud.playerHealth = {50, 0};
        state.hud.playerEnergy = {50, -1};
        state.hud.bossHealth = {50, 0};
        state.progressRatio = 5.0f;
        state.player.visible = false;
        state.enemies.front().visible = false;
        widget.set_game_state(&state);
        QVERIFY(contains_more_than_one_color(render_widget(widget, {1280, 720})));

        widget.set_game_state(nullptr);
        QVERIFY(contains_more_than_one_color(render_widget(widget)));
    }

private:
    QString m_originalWorkingDirectory;
    std::unique_ptr<QTemporaryDir> m_temporaryDirectory;
};

} // namespace alleyfist

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QApplication application(argc, argv);
    alleyfist::ViewSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ViewSmokeTest.moc"
