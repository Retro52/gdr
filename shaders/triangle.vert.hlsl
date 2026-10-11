static const float2 positions[3] = {
    float2( 0.0, -0.5),
    float2(-0.5,  0.5),
    float2( 0.5,  0.5)
};

struct Input
{
    uint vtx_index: SV_VertexID;
};

struct Output
{
    float4 position: SV_Position;
};

[shader("vertex")]
Output main(Input input)
{
    Output result;
    result.position = float4(positions[input.vtx_index], 0.0, 1.0);

    return result;
}
