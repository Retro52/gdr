float3 from_srgb(float3 color)
{
    return pow(color, 2.2.xxx);
}

// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md#pbr-neutral-specification
float3 tonemap(float3 color)
{
    const float f_90 = 0.04F;
    const float k_d = 0.15F;
    const float k_s = 0.8F - f_90;

    float x = min(color.r, min(color.g, color.b));
    float f = x <= 2.0F * f_90 ? (x - x * x / (4 * f_90)) : f_90;

    float3 c_min_f = color - f.xxx;
    float p = max(c_min_f.r, max(c_min_f.g, c_min_f.b));

    float ks1 = 1.0F - k_s;
    float p_n = 1 - ks1 * ks1 / (p + 1 - 2 * k_s);
    float g = 1 / (k_d * (p - p_n) + 1);

    return p <= k_s ? c_min_f : c_min_f * p_n * g / p + p_n.xxx * (1 - g);
}

// https://www.shadertoy.com/view/4tXcWr
float4 from_linear(float4 rgb_linear)
{
    bool4 cutoff = rgb_linear < 0.0031308.xxxx;
    float4 higher = 1.055.xxxx * pow(rgb_linear, (1.0 / 2.4).xxxx) - 0.055.xxxx;
    float4 lower = rgb_linear * 12.92.xxxx;

    return select(cutoff, lower, higher);
}
