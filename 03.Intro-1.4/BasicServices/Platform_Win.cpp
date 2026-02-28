#include "Platform.h"
#include <windows.h>

namespace services {

void Platform::setConsoleColor(ConsoleColor color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default white

    switch(color) {
        case ConsoleColor::Gray:
            attribute = FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::Cyan:
            attribute = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::Green:
            attribute = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::Yellow:
            attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::Red:
            attribute = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::BoldRed:
            attribute = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case ConsoleColor::Reset:
            attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            break;
    }

    SetConsoleTextAttribute(hConsole, attribute);
}

} // namespace services
