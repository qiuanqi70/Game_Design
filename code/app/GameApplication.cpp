#include "GameApplication.h"

namespace alleyfist {

GameApplication::GameApplication(int& argc, char** argv)
    : m_qtApp(argc, argv)
    , m_viewModel()
    , m_mainWindow()
    , m_adapter(m_mainWindow.gameWidget(), &m_viewModel)
{
    QCoreApplication::setApplicationName("Alley Fist");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("AlleyFist");
}

int GameApplication::run()
{
    m_mainWindow.show();
    return m_qtApp.exec();
}

} // namespace alleyfist
