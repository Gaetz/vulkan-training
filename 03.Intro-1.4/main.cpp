#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Create window with Vulkan support
    SDL_Window* window = SDL_CreateWindow(
        "Vulkan with SDL3",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Get required Vulkan extensions from SDL
    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    std::cout << "Required Vulkan extensions for SDL3:" << std::endl;
    for (Uint32 i = 0; i < extensionCount; ++i) {
        std::cout << "  " << extensionNames[i] << std::endl;
    }

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
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
                    std::cout << "Window resized to "
                              << event.window.data1 << "x"
                              << event.window.data2 << std::endl;
                    break;
            }
        }
    }

    // Cleanup
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Application closed cleanly." << std::endl;
    return 0;
}
