#include "app/GameApplication.h"

// 程序入口只进入 App 层，不直接创建 View 或 ViewModel。
// 这样 main 不越过组合根，三层对象的组装关系集中在 GameApplication 内部。
int main(int argc, char** argv)
{
    alleyfist::GameApplication app(argc, argv);
    return app.run();
}
