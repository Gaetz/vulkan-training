#pragma once

#include <SDL3/SDL.h>
#include <string>

class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool init();
    void mainLoop();
    void cleanup();

    [[nodiscard]] SDL_Window* getWindow() const { return window; }
    [[nodiscard]] bool isRunning() const { return running; }

private:
    void processEvents();

    std::string title;
    int width;
    int height;

    SDL_Window* window = nullptr;
    bool running = false;
    bool initialized = false;
};
