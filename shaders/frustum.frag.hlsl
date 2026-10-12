struct Output
{
    float4 out_color: SV_Target0;
};

[shader("pixel")]
Output main()
{
    // TODO: use a specialization constant?
    Output result;
    result.out_color = float4(1.0, 1.0, 0.0, 1.0);

    return result;
}
