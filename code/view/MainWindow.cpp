#include "MainWindow.h"
#include "GameWidget.h"

#include <QApplication>
#include <QScreen>

namespace alleyfist {

namespace {

// View 层内部绑定器：它知道 GameWidget 的信号，但只通过 Common 接口认识 ViewModel。
// 命令绑定：GameWidget -> IGameCommandSink。
// 数据绑定/通知绑定：IGameSnapshotSource 的 change callback -> GameWidget::updateSnapshot。
class GameWidgetBinding final : public QObject {
public:
    GameWidgetBinding(GameWidget* widget,
                      IGameCommandSink& commandSink,
                      IGameSnapshotSource& snapshotSource,
                      QObject* parent = nullptr)
        : QObject(parent)
        , m_widget(widget)
        , m_commandSink(commandSink)
        , m_snapshotSource(snapshotSource)
    {
        connect(m_widget, &GameWidget::commandGenerated,
                this, [this](const GameCommand& command) {
                    m_commandSink.handle_command(command);
                });

        connect(m_widget, &GameWidget::tickRequested,
                this, [this](float deltaSeconds, std::uint64_t frameIndex) {
                    m_commandSink.handle_command(
                        GameCommand::tick_command(deltaSeconds, frameIndex));
                });

        m_snapshotSource.set_change_callback([this](ChangeReason) {
            m_widget->updateSnapshot(m_snapshotSource.snapshot());
        });

        m_widget->updateSnapshot(m_snapshotSource.snapshot());
    }

    ~GameWidgetBinding() override
    {
        m_snapshotSource.set_change_callback({});
    }

private:
    GameWidget* m_widget = nullptr;
    IGameCommandSink& m_commandSink;
    IGameSnapshotSource& m_snapshotSource;
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // 窗口标题
    setWindowTitle("Alley Fist");

    // 默认大小 960×540，可缩放
    resize(960, 540);

    // 居中显示
    if (auto* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move((avail.width() - width()) / 2, (avail.height() - height()) / 2);
    }

    // 最小窗口尺寸
    setMinimumSize(480, 270);

    // 创建 GameWidget 作为中心控件
    m_gameWidget = new GameWidget(this);
    setCentralWidget(m_gameWidget);
}

void MainWindow::bind(IGameCommandSink& commandSink,
                      IGameSnapshotSource& snapshotSource)
{
    delete m_binding;
    m_binding = new GameWidgetBinding(m_gameWidget, commandSink, snapshotSource, this);
}

} // namespace alleyfist
