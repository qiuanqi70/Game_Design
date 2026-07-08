#include "common/common.h"

#include <QApplication>
#include "view/MainWindow.h"
#include "view/ViewAdapter.h"
#include "viewmodel/GameViewModel.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    alleyfist::GameViewModel viewModel;
    alleyfist::MainWindow mainWindow;

    mainWindow.show();

    return app.exec();
}
