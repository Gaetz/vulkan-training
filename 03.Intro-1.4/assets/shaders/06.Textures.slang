// Step 06 — Textures
// UBO at binding 0 (MVP transform, unchanged from Step 05).
// Combined image sampler at binding 1, accessed in the fragment stage.
// Vertex adds texCoord at location 2; fragment outputs the sampled color.

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

[[vk::binding(1, 0)]]
Sampler2D texSampler;

struct VSInput {
    [[vk::location(0)]] float2 position;
    [[vk::location(1)]] float3 color;
    [[vk::location(2)]] float2 texCoord;
};

struct VSOutput {
    float3 color;
    float2 texCoord;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.sv_position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 0.0, 1.0))));
    output.color       = input.color;
    output.texCoord    = input.texCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target
{
    return texSampler.Sample(inVert.texCoord);
}
