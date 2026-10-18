static const float kPI = 3.14159265359;

float3 fresnel_schlick(float h_dot_v, float3 f0)
{
    return f0 + (1.0F - f0) * pow(1.0 - h_dot_v, 5.0F);
}

float3 fresnel_schlick_roughness(float n_dot_v, float3 f0, float roughness)
{
    return f0 + (max((1.0 - roughness).xxx, f0) - f0) * pow(1.0 - n_dot_v, 5.0F);
}

float geometry_schlick_ggx(float n_dot_v, float k)
{
    return n_dot_v / (n_dot_v * (1.0F - k) + k);
}

float geometry_smith(float n_dot_v, float n_dot_l, float k)
{
    return geometry_schlick_ggx(n_dot_v, k) * geometry_schlick_ggx(n_dot_l, k);
}

float3 importance_sample_ggx(float2 xi, float3 nrm, float roughness)
{
    float phi = 2.0 * kPI * xi.x;

    float a = roughness * roughness;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    float3 sc;
    sc.x = cos(phi) * sin_theta;
    sc.y = sin(phi) * sin_theta;
    sc.z = cos_theta;

    float3 up        = abs(nrm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent   = normalize(cross(up, nrm));
    float3 bitangent = cross(nrm, tangent);

    float3 sample_vec = tangent * sc.x + bitangent * sc.y + nrm * sc.z;
    return normalize(sample_vec);
}

float distribution_townbridge_reitz_ggx(float n_dot_h, float alpha)
{
    float a_2 = alpha * alpha;
    float n_dot_h_2 = n_dot_h * n_dot_h;

    float denom = n_dot_h_2 * (a_2 - 1.0F) + 1.0F;
    return a_2 / (kPI * denom * denom);
}

float3 pbr_specular(float3 fresnel, float dist, float geom, float n_dot_v, float n_dot_l)
{
    float denom = 4.0F * n_dot_v * n_dot_l + 0.0001;
    return fresnel * dist * geom / denom;
}

float radical_inverse_vdc(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), radical_inverse_vdc(i));
}

// This code is essentially from godot, but ported to HLSL from C++
float get_perceived_brightness(float3 color)
{
    const float3 kBT601 = float3(0.299F, 0.587F, 0.114F);
    const float3 value = kBT601 * (color * color);

    return sqrt(value.r + value.g + value.b);
}

float solve_metallic(float p_dielectric_specular, float p_diffuse, float p_specular, float p_one_minus_specular_strength)
{
    if (p_specular <= p_dielectric_specular)
    {
        return 0.0f;
    }

    const float a = p_dielectric_specular;
    const float b = p_diffuse * p_one_minus_specular_strength / (1.0F - p_dielectric_specular) + p_specular - 2.0F * p_dielectric_specular;
    const float c = p_dielectric_specular - p_specular;
    const float D = b * b - 4.0F * a * c;
    return clamp((-b + sqrt(D)) / (2.0F * a), 0.0F, 1.0F);
}

float3 orm_from_gloss_spec(float4 spec_gloss_factor, float3 diffuse, out float3 albedo)
{
    const float3 kDielectricSpec = 0.04F.xxx;

    float factor = spec_gloss_factor.a;
    float3 specular = spec_gloss_factor.rgb;

    const float one_minus_specular_strength = 1.0f - max(specular.r, max(specular.g, specular.b));
    const float dielectric_specular_red = kDielectricSpec.r;

    const float brightness_diffuse = get_perceived_brightness(diffuse);
    const float brightness_specular = get_perceived_brightness(specular);

    float roughness = 1.0F - factor;
    float metallic = solve_metallic(kDielectricSpec.r, brightness_diffuse, brightness_specular, one_minus_specular_strength);

    const float one_minus_metallic = 1.0f - metallic;

    const float3 base_color_from_diffuse = diffuse * (one_minus_specular_strength / (1.0F - dielectric_specular_red) / max(one_minus_metallic, 0.00001F));
    const float3 base_color_from_specular = (specular - (kDielectricSpec * one_minus_metallic)) * (1.0F / max(metallic, 0.00001F));
    albedo = clamp(lerp(base_color_from_diffuse, base_color_from_specular, metallic * metallic), 0.0F.xxx, 1.0F.xxx);

    return float3(0.0F, roughness, metallic);
}
