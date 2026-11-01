struct Input
{
    float4 color: TEXCOORD0;
};

struct Output
{
    float4 out_color: SV_Target0;
};

[shader("pixel")]
Output main(Input input)
{
    Output result;
    result.out_color = input.color;

    return result;
}
