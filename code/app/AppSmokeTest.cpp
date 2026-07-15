#include "GameApplication.h"

#include "../view/GameWidget.h"
#include "../view/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include <type_traits>

namespace alleyfist {
namespace {

void send_key_event(QWidget& target, QEvent::Type type, int key)
{
    QKeyEvent event(type, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &event);
}

void click_key(QWidget& target, int key)
{
    send_key_event(target, QEvent::KeyPress, key);
    send_key_event(target, QEvent::KeyRelease, key);
}

QImage render_widget(GameWidget& widget)
{
    widget.resize(640, 360);
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
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

class AppSmokeTest final : public QObject {
    Q_OBJECT

public:
    explicit AppSmokeTest(GameApplication& application)
        : m_application(application)
    {}

private slots:
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

    void applicationMetadataIsConfigured()
    {
        QVERIFY(QCoreApplication::instance() != nullptr);
        QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("Alley Fist"));
        QCOMPARE(QCoreApplication::applicationVersion(), QStringLiteral("0.1.0"));
        QCOMPARE(QCoreApplication::organizationName(), QStringLiteral("AlleyFist"));

        QVERIFY(!std::is_copy_constructible_v<GameApplication>);
        QVERIFY(!std::is_copy_assignable_v<GameApplication>);
    }

    void compositionRootCreatesTheViewObjectGraph()
    {
        QVERIFY(m_mainWindow != nullptr);
        QVERIFY(m_gameWidget != nullptr);
        QVERIFY(!m_mainWindow->isVisible());
        QCOMPARE(m_mainWindow->windowTitle(), QStringLiteral("Alley Fist"));
        QCOMPARE(m_mainWindow->minimumSize(), QSize(480, 270));
        QCOMPARE(m_mainWindow->centralWidget(), static_cast<QWidget*>(m_gameWidget));
        QCOMPARE(m_gameWidget->parentWidget(), static_cast<QWidget*>(m_mainWindow));
        QCOMPARE(m_gameWidget->focusPolicy(), Qt::StrongFocus);
    }

    void propertyCommandsAndNotificationFormACompleteLoop()
    {
        m_mainWindow->show();
        QCoreApplication::processEvents();
        const QImage titleImage = render_widget(*m_gameWidget);

        UpdateRequestCounter updateCounter;
        m_mainWindow->installEventFilter(&updateCounter);

        const auto receivesUpdateFromClick = [&](int key) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            const int before = updateCounter.count;
            click_key(*m_gameWidget, key);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            return updateCounter.count > before;
        };

        // Reset is valid in every phase and puts the application into Playing.
        QVERIFY2(receivesUpdateFromClick(Qt::Key_R),
                 "reset command did not notify the View");
        const QImage playingImage = render_widget(*m_gameWidget);
        QVERIFY(titleImage != playingImage);

        QVERIFY2(receivesUpdateFromClick(Qt::Key_P),
                 "pause command did not notify the View");
        const QImage pausedImage = render_widget(*m_gameWidget);
        QVERIFY(playingImage != pausedImage);

        QVERIFY2(receivesUpdateFromClick(Qt::Key_Return),
                 "confirm command did not resume and notify the View");
        const QImage resumedImage = render_widget(*m_gameWidget);
        QVERIFY(pausedImage != resumedImage);

        // Movement press and release are separate commands and both notify.
        const auto receivesUpdatesFromMovement = [&](int key) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            int before = updateCounter.count;
            send_key_event(*m_gameWidget, QEvent::KeyPress, key);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            const bool pressNotified = updateCounter.count > before;

            before = updateCounter.count;
            send_key_event(*m_gameWidget, QEvent::KeyRelease, key);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
            QCoreApplication::processEvents();
            const bool releaseNotified = updateCounter.count > before;
            return pressNotified && releaseNotified;
        };

        QVERIFY2(receivesUpdatesFromMovement(Qt::Key_A),
                 "move-left binding is missing");
        QVERIFY2(receivesUpdatesFromMovement(Qt::Key_D),
                 "move-right binding is missing");
        QVERIFY2(receivesUpdatesFromMovement(Qt::Key_W),
                 "move-up binding is missing");
        QVERIFY2(receivesUpdatesFromMovement(Qt::Key_S),
                 "move-down binding is missing");

        QVERIFY2(receivesUpdateFromClick(Qt::Key_J),
                 "primary-action binding is missing");
        QVERIFY2(receivesUpdateFromClick(Qt::Key_K),
                 "secondary-action binding is missing");
        QVERIFY2(receivesUpdateFromClick(Qt::Key_Space),
                 "state-toggle binding is missing");

        m_mainWindow->removeEventFilter(&updateCounter);
    }

    void tickBindingFeedsBackThroughNotification()
    {
        m_mainWindow->show();
        m_gameWidget->setRunning(false);
        click_key(*m_gameWidget, Qt::Key_R);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
        QCoreApplication::processEvents();

        UpdateRequestCounter updateCounter;
        m_mainWindow->installEventFilter(&updateCounter);
        const int before = updateCounter.count;

        m_gameWidget->setRunning(true);
        for (int attempt = 0; attempt < 50 && updateCounter.count <= before; ++attempt) {
            QTest::qWait(10);
        }
        m_gameWidget->setRunning(false);
        const bool tickNotified = updateCounter.count > before;

        m_mainWindow->removeEventFilter(&updateCounter);
        QVERIFY2(tickNotified, "tick binding did not notify the View");
    }

    void runShowsTheWindowAndReturnsTheEventLoopCode()
    {
        m_mainWindow->hide();
        QVERIFY(!m_mainWindow->isVisible());

        bool observedVisibleWindow = false;
        QTimer::singleShot(0, qApp, [&]() {
            observedVisibleWindow = m_mainWindow->isVisible();
            QCoreApplication::exit(23);
        });

        QCOMPARE(m_application.run(), 23);
        QVERIFY(observedVisibleWindow);
    }

private:
    GameApplication& m_application;
    MainWindow* m_mainWindow = nullptr;
    GameWidget* m_gameWidget = nullptr;
    int m_mainWindowCount = 0;
};

} // namespace alleyfist

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    const QString originalWorkingDirectory = QDir::currentPath();
    if (!QDir::setCurrent(temporaryDirectory.path())) return 2;
    if (!QDir().mkpath(QStringLiteral("assets/sfx"))) {
        QDir::setCurrent(originalWorkingDirectory);
        return 3;
    }

    int result = 0;
    {
        alleyfist::GameApplication application(argc, argv);
        alleyfist::AppSmokeTest test(application);
        result = QTest::qExec(&test, argc, argv);
    }

    if (!QDir::setCurrent(originalWorkingDirectory) && result == 0) result = 4;
    return result;
}

#include "AppSmokeTest.moc"
