// Step 07 — Depth Buffering
// Same bindings as Step 06 (UBO at 0, combined image sampler at 1).
// Vertex position is now float3 to support two overlapping squares at
// different Z depths. The depth attachment enables correct occlusion.

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
    [[vk::location(0)]] float3 position;
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
    output.sv_position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
    output.color       = input.color;
    output.texCoord    = input.texCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target
{
    return texSampler.Sample(inVert.texCoord);
}
