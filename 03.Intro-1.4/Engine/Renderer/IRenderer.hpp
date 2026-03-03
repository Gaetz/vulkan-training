#pragma once

#include <SDL3/SDL.h>

class IScene;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(SDL_Window* window) = 0;
    virtual void render(IScene& scene) = 0;
    virtual void cleanup() = 0;

    // Called by the Engine before scene->cleanup() to ensure the GPU has
    // finished all in-flight work before scene resources are destroyed.
    virtual void waitIdle() {}

    // Called by the Engine after scene->init() succeeds.
    // Override to set up renderer resources that depend on scene content
    // (e.g. descriptor sets bound to scene textures).
    virtual void onSceneReady(IScene& /*scene*/) {}

    // Called by the Engine when the framebuffer pixel size changes.
    // Default is a no-op; override to trigger swapchain recreation.
    virtual void onWindowResized() {}
};
