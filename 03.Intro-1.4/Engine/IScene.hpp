#pragma once

#include <SDL3/SDL.h>

class IRenderer;

class IScene {
public:
    virtual ~IScene() = default;

    virtual bool init(IRenderer& renderer) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void onEvent(const SDL_Event& event) {}
    virtual void cleanup() = 0;
};
