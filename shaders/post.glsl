vec3 from_srgb(vec3 color)
{
    return pow(color, vec3(2.2));
}

// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md#pbr-neutral-specification
vec3 tonemap(vec3 color)
{
    const float f_90 = 0.04F;
    const float k_d = 0.15F;
    const float k_s = 0.8F - f_90;

    float x = min(color.r, min(color.g, color.b));
    float f = x <= 2.0F * f_90 ? (x - x * x / (4 * f_90)) : f_90;

    vec3 c_min_f = color - vec3(f);
    float p = max(c_min_f.r, max(c_min_f.g, c_min_f.b));

    float ks1 = 1.0F - k_s;
    float p_n = 1 - ks1 * ks1 / (p + 1 - 2 * k_s);
    float g = 1 / (k_d * (p - p_n) + 1);

    return p <= k_s ? c_min_f : c_min_f * p_n * g / p + vec3(p_n) * (1 - g);
}

// https://www.shadertoy.com/view/4tXcWr
vec4 from_linear(vec4 rgb_linear)
{
    bvec4 cutoff = lessThan(rgb_linear, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(rgb_linear, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = rgb_linear * vec4(12.92);

    return mix(higher, lower, cutoff);
}
