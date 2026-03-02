#include "Engine.hpp"
#include "../BasicServices/Log.h"

using services::Log;

Engine::Engine(SDL_Window* window) : window(window) {}

Engine::~Engine() {
    if (initialized) {
        cleanup();
    }
}

void Engine::setRenderer(std::unique_ptr<IRenderer> r) {
    renderer = std::move(r);
}

void Engine::setScene(std::unique_ptr<IScene> s) {
    scene = std::move(s);
}

bool Engine::init() {
    if (!renderer) {
        Log::Error("Engine: no renderer set before init");
        return false;
    }
    if (!renderer->init(window)) {
        Log::Error("Engine: renderer init failed");
        return false;
    }
    if (scene) {
        if (!scene->init(*renderer)) {
            Log::Error("Engine: scene init failed");
            renderer->cleanup();
            return false;
        }
    }
    Log::Info("Engine initialized.");
    initialized = true;
    running     = true;
    return true;
}

void Engine::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                    running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                Log::Debug("Window resized to %dx%d",
                           event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                // Framebuffer pixel size changed (covers Retina/HiDPI too).
                if (renderer) renderer->onWindowResized();
                break;
            default:
                break;
        }
        // Every event is forwarded to the scene so it can react to input.
        if (scene) scene->onEvent(event);
    }
}

void Engine::update(float deltaTime) {
    if (scene) scene->update(deltaTime);
}

void Engine::render() {
    if (renderer && scene) renderer->render(*scene);
}

void Engine::cleanup() {
    if (scene) {
        scene->cleanup();
        scene.reset();
    }
    if (renderer) {
        renderer->cleanup();
        renderer.reset();
    }
    initialized = false;
    running     = false;
    Log::Info("Engine cleaned up.");
}

bool Engine::swapScene(std::unique_ptr<IScene> newScene) {
    if (!initialized) {
        Log::Error("Engine: cannot swap scene before init");
        return false;
    }
    if (scene) scene->cleanup();
    scene = std::move(newScene);
    if (scene && !scene->init(*renderer)) {
        Log::Error("Engine: new scene init failed");
        scene.reset();
        return false;
    }
    Log::Info("Engine: scene swapped.");
    return true;
}

bool Engine::swapRenderer(std::unique_ptr<IRenderer> newRenderer) {
    if (!initialized) {
        Log::Error("Engine: cannot swap renderer before init");
        return false;
    }
    if (scene) scene->cleanup();
    if (renderer) renderer->cleanup();
    renderer = std::move(newRenderer);
    if (!renderer->init(window)) {
        Log::Error("Engine: new renderer init failed");
        renderer.reset();
        initialized = false;
        return false;
    }
    if (scene && !scene->init(*renderer)) {
        Log::Error("Engine: scene re-init failed after renderer swap");
        scene.reset();
        return false;
    }
    Log::Info("Engine: renderer swapped.");
    return true;
}
