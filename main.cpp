#include "app/TextRpgApp.hpp"

int main()
{
    auto config = textrpg::app::parseAppConfig();
    textrpg::app::TextRpgApp app(std::move(config));
    return app.run();
}
