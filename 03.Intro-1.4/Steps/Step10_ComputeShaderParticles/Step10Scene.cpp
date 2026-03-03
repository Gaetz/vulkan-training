#include "Step10Scene.hpp"

#include <cmath>
#include <random>

#include "../../BasicServices/Log.h"

using services::Log;

static constexpr uint32_t PARTICLE_COUNT = 16384; // 64 * 256

// =============================================================================
//  init — generate PARTICLE_COUNT particles scattered in the unit circle,
//         with random velocities and angle-based colors (red-yellow gradient).
// =============================================================================
bool Step10Scene::init(IRenderer& /*renderer*/) {
    particles.resize(PARTICLE_COUNT);

    std::mt19937 rng{42}; // fixed seed for reproducibility
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265358979323846f);
    std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);

    for (auto& p : particles) {
        float angle  = angleDist(rng);
        float radius = std::sqrt(radiusDist(rng)); // sqrt for uniform distribution inside circle

        p.position[0] = radius * std::cos(angle);
        p.position[1] = radius * std::sin(angle);

        // Random normalized velocity * 0.25 speed
        float velAngle = angleDist(rng);
        p.velocity[0]  = std::cos(velAngle) * 0.25f;
        p.velocity[1]  = std::sin(velAngle) * 0.25f;

        // Red-yellow gradient based on position angle
        float t       = (angle / (2.0f * 3.14159265358979323846f));
        p.color[0]    = 1.0f;              // R always full
        p.color[1]    = t;                 // G varies 0→1
        p.color[2]    = 0.0f;
        p.color[3]    = 1.0f;
    }

    Log::Info("Step10Scene: generated %u particles.", PARTICLE_COUNT);
    return true;
}

// =============================================================================
//  update — store delta time for the renderer to read
// =============================================================================
void Step10Scene::update(float dt) {
    deltaTime = dt;
}

// =============================================================================
//  cleanup
// =============================================================================
void Step10Scene::cleanup() {
    particles.clear();
    deltaTime = 0.0f;
}
