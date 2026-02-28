#pragma once

namespace services {

enum class ConsoleColor {
    Gray,
    Cyan,
    Green,
    Yellow,
    Red,
    BoldRed,
    Reset
};

class Platform {
public:
    // Prevent instantiation
    Platform() = delete;
    ~Platform() = delete;
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    // Platform-specific console color setting
    static void setConsoleColor(ConsoleColor color);
};

} // namespace services
