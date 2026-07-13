#include "MainWindow.h"
#include "GameWidget.h"

#include <QApplication>
#include <QScreen>

namespace alleyfist {

// 绑定清理句柄：MainWindow 析构时自动解绑 ViewModel 通知。
struct BindHandle {
    IGameSnapshotSource* source = nullptr;
    BindingCookie cookie = 0;

    ~BindHandle()
    {
        if (source && cookie != 0) {
            source->remove_change_callback(cookie);
        }
    }
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Alley Fist");
    resize(960, 540);

    if (auto* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move((avail.width() - width()) / 2, (avail.height() - height()) / 2);
    }

    setMinimumSize(480, 270);

    auto* widget = new GameWidget(this);
    m_gameWidget = widget;
    setCentralWidget(widget);
}

void MainWindow::bind(IGameCommandSink& commandSink,
                      IGameSnapshotSource& snapshotSource)
{
    auto* widget = m_gameWidget;

    // ---- 数据绑定：推初始快照 ----
    widget->updateSnapshot(snapshotSource.snapshot());

    // ---- 通知绑定：ViewModel 变化 → View 刷新 ----
    auto handle = std::make_unique<BindHandle>();
    handle->source = &snapshotSource;
    handle->cookie = snapshotSource.add_change_callback([widget]() {
        widget->update();
    });

    // ---- 命令绑定：按键 / 定时器 → ViewModel（直接回调，不走 Qt signal） ----
    widget->setCommandCallback([&commandSink](const GameCommand& cmd) {
        commandSink.handle_command(cmd);
    });

    widget->setTickCallback([&commandSink](float dt, std::uint64_t frame) {
        commandSink.handle_command(GameCommand::tick_command(dt, frame));
    });

    m_bindHandle = std::move(handle);
}

} // namespace alleyfist
