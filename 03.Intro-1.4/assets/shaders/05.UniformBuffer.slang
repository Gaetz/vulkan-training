// Step 05 — Uniform Buffer
// MVP transformation matrices are passed via a UBO (descriptor binding 0, set 0).
// Vertex format unchanged from Step 04: 2D position + RGB color.

struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

struct VSInput {
    [[vk::location(0)]] float2 position;
    [[vk::location(1)]] float3 color;
};

struct VSOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.sv_position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 0.0, 1.0))));
    output.color       = input.color;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target
{
    return float4(inVert.color, 1.0);
}
