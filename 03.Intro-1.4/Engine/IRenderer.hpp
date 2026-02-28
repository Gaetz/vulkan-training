#pragma once

#include <SDL3/SDL.h>

class IScene;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(SDL_Window* window) = 0;
    virtual void render(IScene& scene) = 0;
    virtual void cleanup() = 0;
};
