#include "MainWindow.h"
#include "GameWidget.h"

#include <QApplication>
#include <QScreen>

namespace alleyfist {

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

} // namespace alleyfist
