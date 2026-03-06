// Step 11 — GLTF + KTX2
// Same vertex format and pipeline as Step 09 (float3 pos, float3 color, float2 uv).
// The difference is that geometry comes from a GLTF file and the texture from KTX2.

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
