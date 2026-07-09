#include "common/common.h"

#include <QApplication>
#include "view/MainWindow.h"
#include "view/ViewAdapter.h"
#include "viewmodel/GameViewModel.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    // 创建 ViewModel（游戏逻辑）
    alleyfist::GameViewModel viewModel;

    // 创建 View（窗口与渲染）
    alleyfist::MainWindow mainWindow;

    // 创建 Adapter 桥接 View 和 ViewModel
    // 构造函数内部自动连接信号/槽：
    //   GameWidget → commandGenerated/tickRequested → ViewModel::handle_command
    //   ViewModel → changed → GameWidget::updateSnapshot
    alleyfist::ViewAdapter adapter(mainWindow.gameWidget(), &viewModel);

    mainWindow.show();

    return app.exec();
}
