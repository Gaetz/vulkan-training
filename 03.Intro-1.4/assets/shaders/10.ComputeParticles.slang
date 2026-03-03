// Step 10 — Compute Shader Particles
// Compiled twice:
//   Graphics: -entry vertMain -entry fragMain → 10.ComputeParticles.graphics.spv
//   Compute:  -entry compMain                 → 10.ComputeParticles.compute.spv

// =============================================================================
//  Shared data structure
// =============================================================================
struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
};

// =============================================================================
//  Compute shader — updates particle positions each frame
// =============================================================================
[[vk::binding(0)]] cbuffer DeltaTime { float deltaTime; };
[[vk::binding(1)]] StructuredBuffer<Particle>   particlesIn;
[[vk::binding(2)]] RWStructuredBuffer<Particle> particlesOut;

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;

    particlesOut[i].position = particlesIn[i].position + particlesIn[i].velocity * deltaTime;
    particlesOut[i].velocity = particlesIn[i].velocity;
    particlesOut[i].color    = particlesIn[i].color;

    // Bounce off NDC edges [-1, 1]
    if (abs(particlesOut[i].position.x) >= 1.0) particlesOut[i].velocity.x *= -1.0;
    if (abs(particlesOut[i].position.y) >= 1.0) particlesOut[i].velocity.y *= -1.0;
}

// =============================================================================
//  Graphics shaders — render particles as point sprites
// =============================================================================
struct VSIn {
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(1)]] float4 col : COLOR;
};

struct VSOut {
    float4 sv_pos   : SV_Position;
    [[vk::location(0)]] float4 col : COLOR;
    float pointSize : SV_PointSize;
};

[shader("vertex")]
VSOut vertMain(VSIn v) {
    VSOut o;
    o.sv_pos    = float4(v.pos, 0.0, 1.0);
    o.col       = v.col;
    o.pointSize = 4.0;
    return o;
}

[shader("fragment")]
float4 fragMain([[vk::location(0)]] float4 col : COLOR) : SV_Target {
    return col;
}
