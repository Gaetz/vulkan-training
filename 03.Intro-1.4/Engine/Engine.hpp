#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include "Renderer/IRenderer.hpp"
#include "IScene.hpp"

class Engine {
public:
    explicit Engine(SDL_Window* window);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Configure before init()
    void setRenderer(std::unique_ptr<IRenderer> renderer);
    void setScene(std::unique_ptr<IScene> scene);

    bool init();
    void processEvents();
    void update(float deltaTime);
    void render();
    void cleanup();

    // Permutation (only after init())
    bool swapScene(std::unique_ptr<IScene> newScene);
    bool swapRenderer(std::unique_ptr<IRenderer> newRenderer);

    [[nodiscard]] bool isRunning()  const { return running; }
    [[nodiscard]] IRenderer* getRenderer() const { return renderer.get(); }
    [[nodiscard]] IScene* getScene()    const { return scene.get(); }

    void quit() { running = false; }

private:
    SDL_Window* window = nullptr;
    std::unique_ptr<IRenderer> renderer;
    std::unique_ptr<IScene> scene;
    bool initialized = false;
    bool running     = false;
};
