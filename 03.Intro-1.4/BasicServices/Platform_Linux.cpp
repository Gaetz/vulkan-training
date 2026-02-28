#include "Platform.h"
#include <cstdio>

namespace services {

void Platform::setConsoleColor(ConsoleColor color) {
    const char* code = "\033[0m"; // Reset

    switch(color) {
        case ConsoleColor::Gray:
            code = "\033[90m";
            break;
        case ConsoleColor::Cyan:
            code = "\033[36m";
            break;
        case ConsoleColor::Green:
            code = "\033[32m";
            break;
        case ConsoleColor::Yellow:
            code = "\033[33m";
            break;
        case ConsoleColor::Red:
            code = "\033[31m";
            break;
        case ConsoleColor::BoldRed:
            code = "\033[1;31m";
            break;
        case ConsoleColor::Reset:
            code = "\033[0m";
            break;
    }

    printf("%s", code);
}

} // namespace services
