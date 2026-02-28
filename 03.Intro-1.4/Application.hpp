#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include "Engine/Engine.hpp"

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
    [[nodiscard]] bool        isRunning() const { return engine && engine->isRunning(); }
    [[nodiscard]] Engine&     getEngine() const { return *engine; }

private:
    std::string title;
    int width;
    int height;

    SDL_Window* window = nullptr;
    std::unique_ptr<Engine> engine;
    bool initialized = false;
};
