#include "GameApplication.h"

#include "../view/GameWidget.h"
#include "../view/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <initializer_list>
#include <type_traits>

namespace alleyfist {
namespace {

// 应用集成测试统一使用固定画布，并只比较与当前行为相关的屏幕区域。
const QSize kRenderSize{960, 540};
const QRect kPlayerRegion{0, 250, 230, 230};
const QRect kEnergyBarRegion{40, 40, 165, 17};
const QRect kOverlayRegion{200, 110, 560, 330};

// 直接向控件发送键盘事件，绕过真实键盘以便离屏自动化测试。
void send_key_event(QWidget& target, QEvent::Type type, int key)
{
    QKeyEvent event(type, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &event);
}

// 模拟一次完整按键，适用于攻击、暂停、确认和重置等离散命令。
void click_key(QWidget& target, int key)
{
    send_key_event(target, QEvent::KeyPress, key);
    send_key_event(target, QEvent::KeyRelease, key);
}

// 清空 Qt 已投递的刷新事件，保证随后的截图反映最新状态。
void drain_updates()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QCoreApplication::processEvents();
}

// 将 GameWidget 绘制到内存图片，供测试比较界面变化。
QImage render_widget(GameWidget& widget)
{
    widget.resize(kRenderSize);
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

// 记录指定区域内变化的像素数量及其最小包围盒。
struct ImageDifference {
    int changedPixels = 0;
    QRect bounds;
};

// 比较两帧图像，用于判断命令是否造成预期位置上的可见变化。
ImageDifference image_difference(const QImage& before, const QImage& after,
                                 const QRect& requestedRegion)
{
    const QRect region = requestedRegion.intersected(before.rect())
                             .intersected(after.rect());
    ImageDifference result;
    if (region.isEmpty()) return result;

    int minimumX = region.right();
    int minimumY = region.bottom();
    int maximumX = region.left();
    int maximumY = region.top();

    for (int y = region.top(); y <= region.bottom(); ++y) {
        for (int x = region.left(); x <= region.right(); ++x) {
            if (before.pixel(x, y) == after.pixel(x, y)) continue;
            ++result.changedPixels;
            minimumX = std::min(minimumX, x);
            minimumY = std::min(minimumY, y);
            maximumX = std::max(maximumX, x);
            maximumY = std::max(maximumY, y);
        }
    }

    if (result.changedPixels > 0) {
        result.bounds = QRect(QPoint(minimumX, minimumY),
                              QPoint(maximumX, maximumY));
    }
    return result;
}

// 只需数量、不关心变化位置时使用的简化图像比较接口。
int changed_pixel_count(const QImage& before, const QImage& after,
                        const QRect& region)
{
    return image_difference(before, after, region).changedPixels;
}

// 统计 ViewModel 通知最终触发了多少次窗口刷新请求。
class UpdateRequestCounter final : public QObject {
public:
    int count = 0;

protected:
    // 只观察刷新请求，不拦截事件的正常分发。
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::UpdateRequest) ++count;
        return QObject::eventFilter(watched, event);
    }
};

// 保存动作执行前后的画面，以比较动作造型和能量条变化。
struct ActionCapture {
    QImage before;
    QImage after;
};

} // namespace

class AppTest final : public QObject {
    Q_OBJECT

public:
    // 复用真实应用实例，避免测试再创建第二个 QApplication。
    explicit AppTest(GameApplication& application)
        : m_application(application)
    {}

private slots:
    // 测试夹具初始化：确认组合根只创建一个 MainWindow，并取得其 GameWidget。
    void initTestCase()
    {
        const auto topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget* widget : topLevelWidgets) {
            if (auto* window = qobject_cast<MainWindow*>(widget)) {
                ++m_mainWindowCount;
                m_mainWindow = window;
            }
        }

        QCOMPARE(m_mainWindowCount, 1);
        QVERIFY(m_mainWindow != nullptr);
        m_gameWidget = &m_mainWindow->get_game_widget();
        m_gameWidget->setRunning(false);
    }

    // 每个用例后停止计时器、隐藏窗口并清理待处理刷新，避免用例相互影响。
    void cleanup()
    {
        if (m_gameWidget != nullptr) m_gameWidget->setRunning(false);
        if (m_mainWindow != nullptr) m_mainWindow->hide();
        drain_updates();
    }

    // 测试 QApplication 元数据以及 GameApplication 禁止复制和移动的所有权约束。
    void applicationMetadataIsConfigured()
    {
        QCOMPARE(QCoreApplication::instance(),
                 static_cast<QCoreApplication*>(qApp));
        QCOMPARE(QCoreApplication::applicationName(),
                 QStringLiteral("Alley Fist"));
        QCOMPARE(QCoreApplication::applicationVersion(),
                 QStringLiteral("0.1.0"));
        QCOMPARE(QCoreApplication::organizationName(),
                 QStringLiteral("AlleyFist"));

        QVERIFY(!std::is_copy_constructible_v<GameApplication>);
        QVERIFY(!std::is_copy_assignable_v<GameApplication>);
        QVERIFY(!std::is_move_constructible_v<GameApplication>);
        QVERIFY(!std::is_move_assignable_v<GameApplication>);
    }

    // 测试组合根创建的 MainWindow/GameWidget 父子关系、标题、尺寸和焦点配置。
    void compositionRootCreatesTheViewObjectGraph()
    {
        QVERIFY(m_mainWindow != nullptr);
        QVERIFY(m_gameWidget != nullptr);
        QVERIFY(!m_mainWindow->isVisible());
        QCOMPARE(m_mainWindow->windowTitle(), QStringLiteral("Alley Fist"));
        QCOMPARE(m_mainWindow->minimumSize(), QSize(480, 270));
        QCOMPARE(m_mainWindow->centralWidget(),
                 static_cast<QWidget*>(m_gameWidget));
        QCOMPARE(m_gameWidget->parentWidget(),
                 static_cast<QWidget*>(m_mainWindow));
        QCOMPARE(m_gameWidget->minimumSize(), QSize(320, 180));
        QCOMPARE(m_gameWidget->focusPolicy(), Qt::StrongFocus);
    }

    // 测试“按键命令 -> ViewModel 状态变化 -> notification -> View 刷新”的完整绑定闭环。
    void propertyCommandAndNotificationFormACompleteLoop()
    {
        show_window_stopped();
        const QImage title = render_widget(*m_gameWidget);

        UpdateRequestCounter updateCounter;
        m_mainWindow->installEventFilter(&updateCounter);
        drain_updates();
        const int beforeConfirm = updateCounter.count;

        click_key(*m_gameWidget, Qt::Key_Return);
        drain_updates();
        const bool confirmNotified = updateCounter.count > beforeConfirm;
        m_mainWindow->removeEventFilter(&updateCounter);

        const QImage playing = render_widget(*m_gameWidget);
        QVERIFY2(confirmNotified,
                 "confirm changed the model without notifying the View");
        QVERIFY2(changed_pixel_count(title, playing, title.rect()) > 10000,
                 "the bound GameState did not change from Title to Playing");
    }

    // 测试 WASD 四个绑定都能驱动模拟，并让玩家画面向对应方向移动。
    void fourDirectionBindingsMoveThePlayerInTheExpectedDirection()
    {
        show_window_stopped();

        const ImageDifference left = observe_movement(Qt::Key_A);
        const ImageDifference right = observe_movement(Qt::Key_D);
        const ImageDifference up = observe_movement(Qt::Key_W);
        const ImageDifference down = observe_movement(Qt::Key_S);

        for (const ImageDifference* observation :
             {&left, &right, &up, &down}) {
            QVERIFY2(observation->changedPixels > 100,
                     "movement did not produce a visible player-region change");
            QVERIFY(!observation->bounds.isEmpty());
        }

        QVERIFY2(left.bounds.center().x() + 8 < right.bounds.center().x(),
                 "left/right commands are missing or crossed");
        QVERIFY2(up.bounds.center().y() + 8 < down.bounds.center().y(),
                 "up/down commands are missing or crossed");
    }

    // 测试轻击、重击和跳跃绑定会消耗能量，并呈现彼此不同的玩家动作。
    void actionBindingsProduceDistinctActionsAndEnergyCosts()
    {
        show_window_stopped();

        const ActionCapture light = capture_action(Qt::Key_J);
        const ActionCapture heavy = capture_action(Qt::Key_K);
        const ActionCapture jump = capture_action(Qt::Key_Space);

        const int lightEnergyChange = changed_pixel_count(
            light.before, light.after, kEnergyBarRegion);
        const int heavyEnergyChange = changed_pixel_count(
            heavy.before, heavy.after, kEnergyBarRegion);
        const int jumpEnergyChange = changed_pixel_count(
            jump.before, jump.after, kEnergyBarRegion);

        QVERIFY2(lightEnergyChange > 0, "light attack did not spend energy");
        QVERIFY2(heavyEnergyChange > 0, "heavy attack did not spend energy");
        QVERIFY2(jumpEnergyChange > 0, "jump did not spend energy");

        QVERIFY2(changed_pixel_count(
                     light.after, heavy.after, kPlayerRegion) > 100,
                 "light and heavy attacks render the same player action");
        QVERIFY2(changed_pixel_count(
                     light.after, jump.after, kPlayerRegion) > 100,
                 "jump is not reflected in the bound player state");
        QVERIFY2(changed_pixel_count(
                     heavy.after, jump.after, kPlayerRegion) > 100,
                 "heavy attack and jump render the same player action");
    }

    // 测试暂停、确认恢复和重置绑定能进入相应阶段并恢复初始能量状态。
    void pauseConfirmAndResetBindingsRestoreStableStates()
    {
        show_window_stopped();
        reset_to_playing();
        const QImage playing = render_widget(*m_gameWidget);

        click_key(*m_gameWidget, Qt::Key_P);
        drain_updates();
        const QImage paused = render_widget(*m_gameWidget);
        const int pauseDifference = changed_pixel_count(
            playing, paused, kOverlayRegion);
        QVERIFY2(pauseDifference > 5000, "pause overlay was not entered");

        click_key(*m_gameWidget, Qt::Key_Return);
        drain_updates();
        const QImage resumed = render_widget(*m_gameWidget);
        QVERIFY2(changed_pixel_count(playing, resumed, kOverlayRegion) <
                     pauseDifference / 10,
                 "confirm did not resume the paused game");

        click_key(*m_gameWidget, Qt::Key_K);
        drain_updates();
        const QImage modified = render_widget(*m_gameWidget);
        QVERIFY(changed_pixel_count(playing, modified, kEnergyBarRegion) > 0);

        click_key(*m_gameWidget, Qt::Key_R);
        drain_updates();
        const QImage reset = render_widget(*m_gameWidget);
        QCOMPARE(changed_pixel_count(playing, reset, kEnergyBarRegion), 0);
    }

    // 测试 GameWidget 的真实计时器会持续发送 tick，使按住方向键的玩家移动。
    void timerTickDrivesTheBoundSimulation()
    {
        show_window_stopped();
        reset_to_playing();
        const QImage before = render_widget(*m_gameWidget);

        send_key_event(*m_gameWidget, QEvent::KeyPress, Qt::Key_D);
        m_gameWidget->setRunning(true);
        QTest::qWait(180);
        m_gameWidget->setRunning(false);
        drain_updates();

        const QImage after = render_widget(*m_gameWidget);
        const ImageDifference movement = image_difference(
            before, after, kPlayerRegion);
        QVERIFY(movement.changedPixels > 100);
        QVERIFY(!movement.bounds.isEmpty());
        QVERIFY2(movement.bounds.center().x() > 88,
                 "the View timer did not advance the bound simulation");
    }

    // 测试 GameApplication::run 会显示窗口，并原样返回 Qt 事件循环退出码。
    void runShowsTheWindowAndReturnsTheEventLoopCode()
    {
        m_mainWindow->hide();
        QVERIFY(!m_mainWindow->isVisible());

        bool observedVisibleWindow = false;
        QTimer::singleShot(0, qApp, [&]() {
            observedVisibleWindow = m_mainWindow->isVisible();
            m_gameWidget->setRunning(false);
            QCoreApplication::exit(23);
        });

        QCOMPARE(m_application.run(), 23);
        QVERIFY(observedVisibleWindow);
    }

private:
    // 显示测试窗口但停止自动 tick，为需要确定截图的用例建立稳定起点。
    void show_window_stopped()
    {
        m_mainWindow->resize(kRenderSize);
        m_mainWindow->show();
        drain_updates();
        m_gameWidget->setRunning(false);
        drain_updates();
    }

    // 通过真实 R 键绑定把游戏恢复到 Playing 初始状态。
    void reset_to_playing()
    {
        m_gameWidget->setRunning(false);
        click_key(*m_gameWidget, Qt::Key_R);
        drain_updates();
    }

    // 在短时间真实 tick 中按住一个方向键，返回玩家区域的图像变化。
    ImageDifference observe_movement(int key)
    {
        reset_to_playing();
        const QImage before = render_widget(*m_gameWidget);

        send_key_event(*m_gameWidget, QEvent::KeyPress, key);
        m_gameWidget->setRunning(true);
        QTest::qWait(180);
        m_gameWidget->setRunning(false);
        drain_updates();

        const QImage after = render_widget(*m_gameWidget);
        return image_difference(before, after, kPlayerRegion);
    }

    // 从统一初始状态执行一个动作，并捕获动作前后的画面。
    ActionCapture capture_action(int key)
    {
        reset_to_playing();
        ActionCapture capture;
        capture.before = render_widget(*m_gameWidget);
        click_key(*m_gameWidget, key);
        drain_updates();
        capture.after = render_widget(*m_gameWidget);
        return capture;
    }

    GameApplication& m_application;
    MainWindow* m_mainWindow = nullptr;
    GameWidget* m_gameWidget = nullptr;
    int m_mainWindowCount = 0;
};

} // namespace alleyfist

int main(int argc, char** argv)
{
    // 强制使用离屏平台，使测试无需桌面显示环境也能创建和渲染 QWidget。
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    // 使用空临时目录，验证应用不依赖工作目录中的外部资源文件。
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    const QString originalWorkingDirectory = QDir::currentPath();
    if (!QDir::setCurrent(temporaryDirectory.path())) return 2;
    if (!QDir().mkpath(QStringLiteral("assets/sfx"))) {
        QDir::setCurrent(originalWorkingDirectory);
        return 3;
    }

    // GameApplication 必须先于 AppTest 构造，并在恢复工作目录前销毁。
    int result = 0;
    {
        alleyfist::GameApplication application(argc, argv);
        alleyfist::AppTest test(application);
        result = QTest::qExec(&test, argc, argv);
    }

    if (!QDir::setCurrent(originalWorkingDirectory) && result == 0) result = 4;
    return result;
}

#include "AppTest.moc"
