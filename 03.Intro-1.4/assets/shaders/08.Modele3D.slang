// Step 08 — Loading Models
// Identical to Step 07: float3 position, UBO at binding 0, texture at binding 1.
// All vertex colors are set to {1,1,1} by the model loader (white = full texture).

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
