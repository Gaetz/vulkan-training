#include "Application.hpp"
#include "BasicServices/Log.h"
#include <SDL3/SDL_vulkan.h>

using services::Log;

Application::Application(const std::string& title, int width, int height)
    : title(title), width(width), height(height) {}

Application::~Application() {
    if (initialized) {
        cleanup();
    }
}

bool Application::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Error("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow(
        title.c_str(),
        width,
        height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        Log::Error("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    Log::Info("Required Vulkan extensions for SDL3:");
    for (Uint32 i = 0; i < extensionCount; ++i) {
        Log::Info("  %s", extensionNames[i]);
    }

    initialized = true;
    running = true;
    return true;
}

void Application::mainLoop() {
    while (running) {
        processEvents();
    }
}

void Application::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                Log::Debug("Window resized to %dx%d", event.window.data1, event.window.data2);
                break;
            default:
                break;
        }
    }
}

void Application::cleanup() {
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
    initialized = false;
    Log::Info("Application closed cleanly.");
}
