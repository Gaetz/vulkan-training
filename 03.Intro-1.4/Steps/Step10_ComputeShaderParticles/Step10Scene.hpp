#pragma once

#include <vector>

#include "../../Engine/IScene.hpp"

class Step10Renderer;

// Step 10 — Compute Shader Particles
// Scene generates the initial CPU particle data and tracks delta time.
// Renderer owns all GPU resources (SSBOs, pipelines, descriptors).
class Step10Scene : public IScene {
public:
    // Particle layout must match Step10Renderer::Particle exactly
    struct Particle {
        float position[2];  // x, y  (NDC)
        float velocity[2];  // vx, vy
        float color[4];     // r, g, b, a
    };

    bool init(IRenderer& renderer) override;
    void update(float dt) override;
    void cleanup() override;

    float getDeltaTime() const { return deltaTime; }
    const std::vector<Particle>& getParticles() const { return particles; }

private:
    float deltaTime = 0.0f;
    std::vector<Particle> particles;
};
