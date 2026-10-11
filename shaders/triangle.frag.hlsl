struct Output
{
    float4 color: SV_Target0;
};

[shader("pixel")]
Output main()
{
    Output result;
    result.color = float4(1.0, 1.0, 1.0, 1.0);

    return result;
}
