float shadow_pcf(sampler2DArrayShadow map, vec2 uv, float layer, float ref, float texel)
{
    float sum = 0.0F;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            sum += texture(map, vec4(uv + vec2(x, y) * texel, layer, ref));
        }
    }

    return sum / 9.0F;
}

float shadow_cascade_lookup(sampler2DArrayShadow map, ShadowCascadesData scd, uint layer, vec3 pos, vec3 nrm, float n_dot_l)
{
    vec3 offset_pos = pos + nrm * scd.normal_offset * scd.cascades[layer].texel_world_size * (1.0F - n_dot_l);
    vec4 clip = scd.cascades[layer].vp * vec4(offset_pos, 1.0F);

    if (clip.z <= 0.0F)
    {
        return 1.0F;
    }

    vec2 uv = clip.xy * 0.5F + 0.5F;
    return shadow_pcf(map, uv, float(layer), min(clip.z + scd.depth_bias, 1.0F), 1.0F / scd.sm_resolution);
}

uint get_shadow_cascade_index(ShadowCascadesData scd, float view_depth)
{
    for (uint i = 0; i < kMaxShadowCascades; ++i)
    {
        if (view_depth < scd.cascades[i].split)
        {
            return i;
        }
    }

    return kMaxShadowCascades - 1;
}

float get_shadow(sampler2DArrayShadow map, ShadowCascadesData scd, vec3 pos, vec3 nrm, float view_depth, float n_dot_l)
{
    if (view_depth >= scd.max_range)
    {
        return 1.0F;
    }

    uint layer = get_shadow_cascade_index(scd, view_depth);

    float far_split = scd.cascades[layer].split;
    float near_split = layer == 0 ? 0.0F : scd.cascades[layer - 1].split;

    float band = max((far_split - near_split) * scd.blend_ratio, 1e-4F);
    float blend = clamp((view_depth - (far_split - band)) / band, 0.0F, 1.0F);

    float shadow = shadow_cascade_lookup(map, scd, layer, pos, nrm, n_dot_l);
    if (blend <= 0.0F)
    {
        return shadow;
    }

    float next = layer + 1 < kMaxShadowCascades
        ? shadow_cascade_lookup(map, scd, layer + 1, pos, nrm, n_dot_l)
        : 1.0F;

    return mix(shadow, next, blend);
}
