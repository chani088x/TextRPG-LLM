#include "app/TextRpgApp.hpp"

#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void configureConsoleWindow()
{
#ifdef _WIN32
    const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(output, &mode)) {
        return;
    }

    SetConsoleTitleW(L"LLM Text RPG");
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

#endif
}

} // namespace

int main()
{
    configureConsoleWindow();

    auto config = textrpg::app::parseAppConfig();
    textrpg::app::TextRpgApp app(std::move(config));
    return app.run();
}
