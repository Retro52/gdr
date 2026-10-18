#include "include/shaders/types.hlsl.h"

#include "utils/pbr.hlsl"
#include "utils/math.hlsl"

[[vk::image_format("unknown")]] RWTexture2D<float4> o_color:  register(u0);
[[vk::image_format("unknown")]] RWTexture2D<uint2>  i_visbuf: register(u1);

StructuredBuffer<uint>   indices:  register(t2);
StructuredBuffer<Vertex> vertices: register(t3);

StructuredBuffer<Meshlet> meshlets:     register(t4);
ByteAddressBuffer         meshlet_data: register(t5);  // u32 vertices followed by u8 indices

StructuredBuffer<MeshData>           primitives:      register(t6);
StructuredBuffer<MeshInstance>       instances:       register(t7);
StructuredBuffer<MeshMaterial>       materials:       register(t8);
StructuredBuffer<FrameWorldData>     world:           register(t9);

SamplerState textures_sampler: register(s10);

[[vk::combinedImageSampler]] Texture2D    depth_texture:         register(t11);
[[vk::combinedImageSampler]] SamplerState depth_texture_sampler: register(s11);

[[vk::combinedImageSampler]] Texture2D              brdf_lut:                register(t12);
[[vk::combinedImageSampler]] SamplerState           brdf_lut_sampler:        register(s12);
[[vk::combinedImageSampler]] TextureCube            sky_cubemap:             register(t13);
[[vk::combinedImageSampler]] SamplerState           sky_cubemap_sampler:     register(s13);
[[vk::combinedImageSampler]] TextureCube            irradiance_map:          register(t14);
[[vk::combinedImageSampler]] SamplerState           irradiance_map_sampler:  register(s14);
[[vk::combinedImageSampler]] TextureCube            prefiltered_map:         register(t15);
[[vk::combinedImageSampler]] SamplerState           prefiltered_map_sampler: register(s15);

#if 0
StructuredBuffer<ShadowCascadesData> shadow_cascades: register(t10);
[[vk::combinedImageSampler]] Texture2DArray         shadow_map:              register(t17);
[[vk::combinedImageSampler]] SamplerComparisonState shadow_map_sampler:      register(s17);
#endif

Texture2D textures[]: register(t0, space1);

[[vk::push_constant]] ConstantBuffer<ResolvePassPushConstants> pc: register(b0);

#define TEXTURE2DG(id, uv, dfdx, dfdy) textures[NonUniformResourceIndex(id)].SampleGrad(textures_sampler, uv, dfdx, dfdy)
#define DTEXTURE2D(id, uv, dfdx, dfdy) (id > 0 ? textures[NonUniformResourceIndex(id)].SampleGrad(textures_sampler, uv, dfdx, dfdy) : float4(1.0F, 0.25F, 0.95F, 1.0F))

// https://stackoverflow.com/questions/23319289/is-there-a-good-glsl-hash-function
uint lowbias32(uint x)
{
    x = x == 0 ? 0x100203 : x;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float3 uint_color(uint num)
{
    return float3(float(num & 0xFF) / 255.0F,
    float((num >> 8) & 0xFF) / 255.0F,
    float((num >> 16) & 0xFF) / 255.0F
    );
}

float3 hash_color(uint id)
{
    return uint_color(lowbias32(id));
}

void get_vertex_normal_tangent(uint index, out float3 normal, out float4 tangent)
{
    float4 up_norm = unpack_r10g10b10a2(vertices[index].packed_normal);
    float3 up_tang = decode_oct(unpack_r8g8(uint(vertices[index].packed_tangent)));

    normal = up_norm.xyz;
    tangent = float4(up_tang, up_norm.w);
}

float2 to_ndc(float2 px, float2 size)
{
    return ((px + 0.5F.xx) / size) * 2.0F - 1.0F;
}

void weight_normal_tangent(uint i0, uint i1, uint i2, float4 quat, float3 weights, out float3 normal, out float4 tangent)
{
    float3 n0, n1, n2;
    float4 t0, t1, t2;

    get_vertex_normal_tangent(i0, n0, t0);
    get_vertex_normal_tangent(i1, n1, t1);
    get_vertex_normal_tangent(i2, n2, t2);

    normal = normalize(quat_rotate_vec3(wsum(n0, n1, n2, weights), quat));
    tangent = float4(normalize(quat_rotate_vec3(wsum(t0.xyz, t1.xyz, t2.xyz, weights), quat)), t0.w);
}

float3 get_sky_direction(float2 pixel, int2 image_size)
{
    float2 ndc = to_ndc(pixel, float2(image_size));
    float4 p = mul(pc.vp_inverse, float4(ndc, 1.0F, 1.0F));
    return normalize(p.xyz / p.w);
}

float get_sun_disc_coverage(float3 view_dir, float3 sun_dir, float half_angle, float pixel_angle)
{
    float inner = max(half_angle - 0.5F * pixel_angle, 0.0F);
    float outer = half_angle + 0.5F * pixel_angle;

    float cos_theta = dot(view_dir, sun_dir);
    return smoothstep(cos(outer), cos(inner), cos_theta);
}

#if 0
float shadow_pcf(float2 uv, float layer, float ref, float sm_res)
{
    float2 tc   = uv * sm_res;
    float2 base = floor(tc - 0.5F) + 0.5F;
    float2 f    = tc - base;

    float4 wx = float4(1.0F - f.x, 1.0F, 1.0F, f.x);
    float4 wy = float4(1.0F - f.y, 1.0F, 1.0F, f.y);

    float3 g = float3(base / sm_res, layer);

    float4 s00 = shadow_map.GatherCmp(shadow_map_sampler, g, ref, int2(-1, -1));
    float4 s10 = shadow_map.GatherCmp(shadow_map_sampler, g, ref, int2(1, -1));
    float4 s01 = shadow_map.GatherCmp(shadow_map_sampler, g, ref, int2(-1, 1));
    float4 s11 = shadow_map.GatherCmp(shadow_map_sampler, g, ref, int2(1, 1));

    float sum =
        dot(s00, float4(wx.x * wy.y, wx.y * wy.y, wx.y * wy.x, wx.x * wy.x))
        + dot(s10, float4(wx.z * wy.y, wx.w * wy.y, wx.w * wy.x, wx.z * wy.x))
        + dot(s01, float4(wx.x * wy.w, wx.y * wy.w, wx.y * wy.z, wx.x * wy.z))
        + dot(s11, float4(wx.z * wy.w, wx.w * wy.w, wx.w * wy.z, wx.z * wy.z));

    return sum / 9.0F;
}

float shadow_cascade_lookup(uint layer, float3 pos, float3 nrm, float n_dot_l)
{
    float texel_world_size = shadow_cascades[0].cascades[layer].texel_world_size;

    float3 offset_pos = pos + nrm * shadow_cascades[0].normal_offset * texel_world_size * (1.0F - n_dot_l);
    float4 clip       = mul(shadow_cascades[0].cascades[layer].vp, float4(offset_pos, 1.0F));

    if (clip.z <= 0.0F)
    {
        return 1.0F;
    }

    float2 uv = clip.xy * 0.5F + 0.5F;
    return shadow_pcf(uv,
                      float(layer),
                      min(clip.z + shadow_cascades[0].depth_bias, 1.0F),
                      shadow_cascades[0].sm_resolution);
}

uint get_shadow_cascade_index(float view_depth)
{
    for (uint i = 0; i < kMaxShadowCascades; ++i)
    {
        if (view_depth < shadow_cascades[0].cascades[i].split)
        {
            return i;
        }
    }

    return kMaxShadowCascades - 1;
}
#endif

float get_shadow(float3 pos, float3 nrm, float view_depth, float n_dot_l)
{
#if 0
    if (view_depth >= shadow_cascades[0].max_range)
    {
        return 1.0F;
    }

    uint layer = get_shadow_cascade_index(view_depth);

    float far_split  = shadow_cascades[0].cascades[layer].split;
    float near_split = layer == 0 ? 0.0F : shadow_cascades[0].cascades[layer - 1].split;

    float band  = max((far_split - near_split) * shadow_cascades[0].blend_ratio, 1e-4F);
    float blend = clamp((view_depth - (far_split - band)) / band, 0.0F, 1.0F);

    float shadow = shadow_cascade_lookup(layer, pos, nrm, n_dot_l);
    if (blend <= 0.0F)
    {
        return shadow;
    }

    float next = layer + 1 < kMaxShadowCascades ? shadow_cascade_lookup(layer + 1, pos, nrm, n_dot_l) : 1.0F;
    return lerp(shadow, next, blend);
#endif
    return 1.0F;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int2 image_size = world[0].render_resolution;
    int2 pixel = int2(global_id.xy);

    // the dispatch rounds the group count up, so without this the padding threads read
    // zeroes out of the visbuffer and run the whole geometry path on garbage
    if (any(pixel >= image_size))
    {
        return;
    }

    float2 px = float2(pixel);

    float2 vtp = to_ndc(px, float2(image_size));
    uint2 data = i_visbuf[pixel].xy;

    uint instance_id = data.r & kInstanceIdMask;
    const float exposure = 1.0 / (1.2 * exp2(world[0].camera_exposure));
    if (instance_id == kInstanceIdMask)
    {
        const float kSunAngularDiameter = radians(0.5357F);
        const float kSunHalfAngle = 0.5F * kSunAngularDiameter;
        const float kProjectedSolidAngle = kPI * sin(kSunHalfAngle) * sin(kSunHalfAngle);

        float3 dir   = get_sky_direction(px, image_size);
        float3 dir_x = get_sky_direction(px + float2(1.0F, 0.0F), image_size);
        float3 dir_y = get_sky_direction(px + float2(0.0F, 1.0F), image_size);

        float pixel_angle = max(
            acos(clamp(dot(dir, dir_x), -1.0F, 1.0F)),
            acos(clamp(dot(dir, dir_y), -1.0F, 1.0F))
        );

        float3 sky = sky_cubemap.SampleLevel(sky_cubemap_sampler, dir, 0.0F).rgb * world[0].envmap_scale;
        float3 sun_tint = world[0].sun_color.rgb /
            max(dot(world[0].sun_color.rgb, float3(0.2126F, 0.7152F, 0.0722F)), 1e-4F);

        float disc_luminance = world[0].sun_color.w / kProjectedSolidAngle;
        float disc_coverage = get_sun_disc_coverage(dir, world[0].sun_direction, kSunHalfAngle, pixel_angle);

        float3 color = sky + sun_tint * disc_luminance * disc_coverage;
        o_color[pixel] = float4(color * exposure, 1.0F);
        return;
    }

    float4 inst_pos_scale = instances[instance_id].pos_and_scale;
    float4 inst_quat      = instances[instance_id].rotation_quat;
    uint material_id      = instances[instance_id].material_index;
    uint base_vertex      = instances[instance_id].base_vertex;

    inst_pos_scale.xyz -= world[0].camera_pos;

#ifdef FOR_MESH_PIPELINE
    uint m_idx = data.g >> 7;
    uint ml = (data.g & 127) * 3;

    const uint meshlet_data_offset = meshlets[m_idx].data_offset;
    const uint meshlet_base_vertex = meshlet_data_offset / 4;
    const uint meshlet_base_index  = meshlet_data_offset + meshlets[m_idx].vertices_count() * 4;

    uint i0 = meshlet_data.Load((meshlet_base_vertex + load_u8(meshlet_data, meshlet_base_index + ml + 0)) * 4) + base_vertex;
    uint i1 = meshlet_data.Load((meshlet_base_vertex + load_u8(meshlet_data, meshlet_base_index + ml + 1)) * 4) + base_vertex;
    uint i2 = meshlet_data.Load((meshlet_base_vertex + load_u8(meshlet_data, meshlet_base_index + ml + 2)) * 4) + base_vertex;

#else
    uint i0 = indices[data.g + 0] + base_vertex;
    uint i1 = indices[data.g + 1] + base_vertex;
    uint i2 = indices[data.g + 2] + base_vertex;
#endif

    float3 v0 = transform_vec3(float3(vertices[i0].px, vertices[i0].py, vertices[i0].pz), inst_pos_scale, inst_quat);
    float3 v1 = transform_vec3(float3(vertices[i1].px, vertices[i1].py, vertices[i1].pz), inst_pos_scale, inst_quat);
    float3 v2 = transform_vec3(float3(vertices[i2].px, vertices[i2].py, vertices[i2].pz), inst_pos_scale, inst_quat);

    float4 vt0 = mul(pc.vp, float4(v0, 1.0F));
    float4 vt1 = mul(pc.vp, float4(v1, 1.0F));
    float4 vt2 = mul(pc.vp, float4(v2, 1.0F));

    BaryDeriv bary = calc_full_bary(vt0, vt1, vt2, vtp, float2(image_size));

    float3 normal;
    float4 tangent;
    weight_normal_tangent(i0, i1, i2, inst_quat, bary.lambda, normal, tangent);

    float2 uv0 = float2(vertices[i0].ux, vertices[i0].uy);
    float2 uv1 = float2(vertices[i1].ux, vertices[i1].uy);
    float2 uv2 = float2(vertices[i2].ux, vertices[i2].uy);

    float2 uv   = wsum(uv0, uv1, uv2, bary.lambda);
    float2 dfdx = wsum(uv0, uv1, uv2, bary.ddx);
    float2 dfdy = wsum(uv0, uv1, uv2, bary.ddy);

    uint albedo_idx = materials[material_id].albedo_idx;
    uint normal_idx = materials[material_id].normal_idx;
    uint met_roughness_idx = materials[material_id].met_roughness_idx;
    uint material_bits = uint(materials[material_id].material_bits);

    float4 diffuse_factor = materials[material_id].diffuse_factor;
    float4 orm_factor = materials[material_id].met_roughness_factor;

    float4 frag_col = albedo_idx > 0 ? TEXTURE2DG(albedo_idx, uv, dfdx, dfdy) * diffuse_factor : diffuse_factor;

#if SHADERS_DEBUG
    float3 albedo;
    switch (world[0].debug_mode)
    {
        case kDebugModeLit:
        case kDebugModeLitDiffuse:
        case kDebugModeLitAmbient:
        case kDebugModeLitSpecular:
            albedo = 1.0F.xxx;
            break;
        default:
            albedo = frag_col.rgb;
            break;
    }
#else
    float3 albedo = frag_col.rgb;
#endif

    float3 tex_normal = float3(0, 0, 1);
    if (normal_idx > 0)
        tex_normal = TEXTURE2DG(normal_idx, uv, dfdx, dfdy).rgb * 2 - 1;

    float3 bitangent = cross(normal, tangent.xyz) * tangent.w;

    bool is_front_face = GET_BIT(data.r, 31) == 0;
    float3 nrm = (normal_idx > 0 ? normalize(tex_normal.r * tangent.xyz + tex_normal.g * bitangent + tex_normal.b * normal) : normal) * (is_front_face ? 1.0F : -1.0F);

#if 1
    float3 orm = 1.0F.xxx;
    if (GET_BIT(material_bits, kMatGlossBit) == 1)
    {
        float3 diffuse = albedo;
        float4 sp_gloss = met_roughness_idx > 0
                            ? TEXTURE2DG(met_roughness_idx, uv, dfdx, dfdy) * orm_factor
                            : orm_factor;

        orm = orm_from_gloss_spec(sp_gloss, diffuse, albedo);
    }
    else
    {
        orm = float4(met_roughness_idx > 0 ? TEXTURE2DG(met_roughness_idx, uv, dfdx, dfdy) * orm_factor : orm_factor).xyz;
    }
#else
    float3 orm = float4(met_roughness_idx > 0 ? TEXTURE2DG(met_roughness_idx, uv, dfdx, dfdy) * orm_factor : orm_factor).xyz;
#endif

    float3 view_pos = wsum(v0, v1, v2, bary.lambda);

    float3 view = normalize(-view_pos);
    float3 halfw = normalize(world[0].sun_direction + view);

    float metallic = orm.b;
    float roughness = max(orm.g, 0.045);

    float n_dot_v = max(dot(nrm, view), 0.0F);
    float n_dot_h = max(dot(nrm, halfw), 0.0F);
    float h_dot_v = max(dot(halfw, view), 0.0F);
    float n_dot_l = max(dot(nrm, world[0].sun_direction), 0.0F);

    float alpha = roughness * roughness;

    // Diffuse + specular diffuse
    float k_d = (roughness + 1.0F) * (roughness + 1.0F) / 8.0F;
    float geom = geometry_smith(n_dot_v, n_dot_l, k_d);

    float ndf = distribution_townbridge_reitz_ggx(n_dot_h, alpha);

    float3 f0 = lerp(0.04F.xxx, albedo, metallic);
    float3 frensel_d = fresnel_schlick(h_dot_v, f0);

    float view_depth = mul(pc.vp, float4(view_pos, 1.0F)).w;
    float shadow_factor = get_shadow(view_pos, nrm, view_depth, n_dot_l);

    float3 spec_direct = pbr_specular(frensel_d, ndf, geom, n_dot_v, n_dot_l);
    float3 sun_radiance = world[0].sun_color.rgb * world[0].sun_color.w;
    float3 diffuse = (1.0F.xxx - frensel_d) * (1.0F - metallic);
    float3 direct = (diffuse * albedo / kPI + spec_direct) * sun_radiance * n_dot_l * shadow_factor;

    // Ambient + specular ambient
    float3 fresnel_a = fresnel_schlick_roughness(n_dot_v, f0, roughness);
    float3 ambient = (1.0F.xxx - fresnel_a) * (1.0F - metallic);

    float3 irradiance = irradiance_map.SampleLevel(irradiance_map_sampler, nrm, 0.0F).rgb;
    float3 ambient_diffuse = irradiance * albedo * ambient * world[0].envmap_scale;

    float3 rv = reflect(-view, nrm);
    const float kMaxReflectionSampleLod = float(kEnvPrefilterMips - 1);
    float3 env_prefiltered_color = prefiltered_map.SampleLevel(prefiltered_map_sampler, rv,  roughness * kMaxReflectionSampleLod).rgb;
    float2 env_brdf  = brdf_lut.SampleLevel(brdf_lut_sampler, float2(n_dot_v, roughness), 0.0F).rg;

    float3 ambient_specular =  env_prefiltered_color * (f0 * env_brdf.x + 1.0.xxx * env_brdf.y) * world[0].envmap_scale;

    float3 color = (direct + ambient_diffuse + ambient_specular) * exposure;
    float4 result = float4(color, 1.0F);

#if SHADERS_DEBUG
    if (world[0].debug_mode != kDebugModeShaded)
    {
        if (world[0].debug_mode == kDebugModeLitDiffuse)     { color = direct * exposure; }
        if (world[0].debug_mode == kDebugModeLitAmbient)     { color = ambient_diffuse * exposure; }
        if (world[0].debug_mode == kDebugModeLitSpecular)    { color = ambient_specular * exposure; }
        if (world[0].debug_mode == kDebugModeUV)             { color = float3(uv, 0.0F); }
        if (world[0].debug_mode == kDebugModeNormal)         { color = nrm; }
        if (world[0].debug_mode == kDebugModeTangent)        { color = tangent.xyz; }
        if (world[0].debug_mode == kDebugModeDepth)          { color = depth_texture.SampleLevel(depth_texture_sampler, px / float2(image_size), 0.0F).rgb; }
        if (world[0].debug_mode == kDebugModeWorldPos)       { color = normalize(view_pos); }
        if (world[0].debug_mode == kDebugModeColor)          { color = diffuse_factor.rgb; }
        if (world[0].debug_mode == kDebugModeMetallic)       { color = metallic.xxx; }
        if (world[0].debug_mode == kDebugModeRoughness)      { color = roughness.xxx; }
        if (world[0].debug_mode == kDebugModeAlbedoTexture)  { color = DTEXTURE2D(albedo_idx, uv, dfdx, dfdy).rgb; }
        if (world[0].debug_mode == kDebugModeNormalTexture)  { color = DTEXTURE2D(normal_idx, uv, dfdx, dfdy).rgb; }
        if (world[0].debug_mode == kDebugModeOrmTexture)     { color = DTEXTURE2D(met_roughness_idx, uv, dfdx, dfdy).rgb; }
        if (world[0].debug_mode == kDebugModeTriangleId)     { color = hash_color(i0); }
        if (world[0].debug_mode == kDebugModeInstanceId)     { color = hash_color(instance_id); }
        if (world[0].debug_mode == kDebugModeTriangleFace)   { color = hash_color(uint(is_front_face)); }
#if 0
        if (world[0].debug_mode == kDebugModeShadow)         { color = shadow_factor.xxx; }
        if (world[0].debug_mode == kDebugModeShadowCascades) { color = lerp(color, hash_color(view_depth >= shadow_cascades[0].max_range ? kMaxShadowCascades : get_shadow_cascade_index(view_depth)), 0.5F); }
#endif
        result = float4(color, 1.0F);
    }
#endif

    o_color[pixel] = result;
}
