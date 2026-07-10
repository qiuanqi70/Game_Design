#include "GameApplication.h"

// App 层烟测只验证应用入口能完成组装。
// 不再通过 app 暴露 mainWindow()/viewModel() 去探内部对象，避免测试反向鼓励层间耦合。
int main(int argc, char** argv)
{
    alleyfist::GameApplication app(argc, argv);
    return 0;
}
