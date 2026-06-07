const float kPI = 3.14159265359;

vec3 fresnel_schlick(float h_dot_v, vec3 f0)
{
    return f0 + (1.0F - f0) * pow(1.0 - h_dot_v, 5.0F);
}

float geometry_schlick_ggx(float n_dot_v, float k)
{
    return n_dot_v / (n_dot_v * (1.0F - k) + k);
}

float geometry_smith(float n_dot_v, float n_dot_l, float k)
{
    return geometry_schlick_ggx(n_dot_v, k) * geometry_schlick_ggx(n_dot_l, k);
}

float distribution_townbridge_reitz_ggx(float n_dot_h, float alpha)
{
    float a_2 = alpha * alpha;
    float n_dot_h_2 = n_dot_h * n_dot_h;

    float denom = n_dot_h_2 * (a_2 - 1.0F) + 1.0F;
    return a_2 / (kPI * denom * denom);
}

vec3 pbr_specular(vec3 fresnel, float dist, float geom, float n_dot_v, float n_dot_l)
{
    float denom = 4.0F * n_dot_v * n_dot_l + 0.0001;
    return fresnel * dist * geom / denom;
}
