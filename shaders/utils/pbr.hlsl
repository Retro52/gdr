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
