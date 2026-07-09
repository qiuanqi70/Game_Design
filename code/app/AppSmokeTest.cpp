#include "GameApplication.h"

int main(int argc, char** argv)
{
    alleyfist::GameApplication app(argc, argv);
    if (app.main_window().gameWidget() == nullptr) {
        return 1;
    }

    app.view_model().handle_command(
        alleyfist::GameCommand::tick_command(1.0f / 60.0f, 1));

    return app.view_model().snapshot().frameIndex == 1 ? 0 : 2;
}
