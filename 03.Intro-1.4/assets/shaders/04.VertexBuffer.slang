// Step 04 — Vertex Buffer
// Vertex data is now fed from a GPU buffer (position + color per vertex).
// Locations must match VkVertexInputAttributeDescription::location in C++.

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
    output.sv_position = float4(input.position, 0.0, 1.0);
    output.color       = input.color;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput inVert) : SV_Target
{
    return float4(inVert.color, 1.0);
}
