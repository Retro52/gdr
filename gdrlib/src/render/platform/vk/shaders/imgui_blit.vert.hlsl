struct Input
{
    uint vertex_id: SV_VertexID;
};

struct Output
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 out_uv: TEXCOORD0;
};

[shader("vertex")]
Output main(Input input)
{
    Output result;
    result.out_uv = float2((input.vertex_id << 1) & 2, input.vertex_id & 2);
    result.position = float4(result.out_uv.x * 2.0 - 1.0, 1.0 - result.out_uv.y * 2.0, 0.0, 1.0);

    return result;
}
