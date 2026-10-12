#include "utils/pbr.hlsl"
#include "include/shaders/types.hlsl"

// TODO(dx12): D3D12 has no combined image samplers, the texture and its sampler are bound separately there (t0 + s0)
#ifdef UNPACK_MAIN
[[vk::combinedImageSampler]] Texture2D    i_equirect:         register(t0);
[[vk::combinedImageSampler]] SamplerState i_equirect_sampler: register(s0);

[[vk::image_format("unknown")]] RWTexture2DArray<float4> o_cubemap: register(u1);
#endif

#ifdef CONVOLUTE_MAIN
[[vk::combinedImageSampler]] TextureCube  i_cubemap:         register(t0);
[[vk::combinedImageSampler]] SamplerState i_cubemap_sampler: register(s0);

[[vk::image_format("unknown")]] RWTexture2DArray<float4> o_convolution: register(u1);
#endif

#ifdef PREFILTER_MAIN
[[vk::combinedImageSampler]] TextureCube  i_cubemap:         register(t0);
[[vk::combinedImageSampler]] SamplerState i_cubemap_sampler: register(s0);

[[vk::image_format("unknown")]] RWTexture2DArray<float4> o_prefilter: register(u1);
#endif

struct PushConstants
{
    int resolution;
#ifdef CONVOLUTE_MAIN
    int env_resolution;
#endif

#ifdef PREFILTER_MAIN
    int env_resolution;
    float roughness;
#endif
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

static const float3 kFaceForward[6] = {
    float3( 1,  0,  0),
    float3(-1,  0,  0),
    float3( 0,  1,  0),
    float3( 0, -1,  0),
    float3( 0,  0,  1),
    float3( 0,  0, -1)
};

static const float3 kFaceRight[6] = {
    float3( 0,  0, -1),
    float3( 0,  0,  1),
    float3( 1,  0,  0),
    float3( 1,  0,  0),
    float3( 1,  0,  0),
    float3(-1,  0,  0)
};

static const float3 kFaceUp[6] = {
    float3( 0, -1,  0),
    float3( 0, -1,  0),
    float3( 0,  0,  1),
    float3( 0,  0, -1),
    float3( 0, -1,  0),
    float3( 0, -1,  0)
};

float2 to_ndc(float2 px, float2 size)
{
    return ((px + 0.5F.xx) / size) * 2.0F - 1.0F;
}

float2 to_equirect(float3 dir)
{
    return float2(
        atan2(dir.z, dir.x) * (0.5F / kPI) + 0.5F,
        acos(clamp(dir.y, -1.0F, 1.0F)) / kPI
    );
}

#ifdef UNPACK_MAIN
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int3 image_size = pc.resolution.xxx;
    if (any(global_id.xy >= uint2(image_size.xy)))
        return;

    uint face = global_id.z;

    float2 pixel = float2(global_id.xy);
    float2 uv = to_ndc(pixel, float2(image_size.xy));

    float3 dir = normalize(kFaceForward[face] + uv.x * kFaceRight[face] + uv.y * kFaceUp[face]);
    float4 color = i_equirect.SampleLevel(i_equirect_sampler, to_equirect(dir), 0.0F);

    // note to self -- this min is not arbitrary, this is to avoid double suns illumination in a scene
    // maybe, it would make sense to pass it as a parameter of some sort, but for now this will do
    o_cubemap[global_id.xyz] = min(color, 10.0F.xxxx);
}
#endif

#ifdef CONVOLUTE_MAIN1
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int3 image_size = pc.resolution.xxx;
    if (any(global_id.xy >= uint2(image_size.xy)))
        return;

    uint face = global_id.z;

    float2 pixel = float2(global_id.xy);
    float2 uv = to_ndc(pixel, float2(image_size.xy));

    float3 dir   = normalize(kFaceForward[face] + uv.x * kFaceRight[face] + uv.y * kFaceUp[face]);
    float3 right = normalize(kFaceRight[face] - dir * dot(dir, kFaceRight[face]));
    float3 up    = cross(dir, right);
    float3 irradiance = 0.0.xxx;

    float sample_delta = 0.025;
    float total_samples = 0.0;
    for(float phi = 0.0; phi < 2.0 * kPI; phi += sample_delta)
    {
        for(float theta = 0.0; theta < 0.5 * kPI; theta += sample_delta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangent_sample = float3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 vec_sample = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * dir;

            irradiance += i_cubemap.SampleLevel(i_cubemap_sampler, vec_sample, 0.0F).rgb * cos(theta) * sin(theta);
            total_samples++;
        }
    }

    irradiance = kPI * irradiance * (1.0 / float(total_samples));
    o_convolution[global_id.xyz] = float4(irradiance, 1.0);
}
#endif

#ifdef CONVOLUTE_MAIN
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int3 image_size = pc.resolution.xxx;
    if (any(global_id.xy >= uint2(image_size.xy)))
    return;

    uint face = global_id.z;

    float2 pixel = float2(global_id.xy);
    float2 uv = to_ndc(pixel, float2(image_size.xy));

    float3 n = normalize(kFaceForward[face] + uv.x * kFaceRight[face] + uv.y * kFaceUp[face]);

    float sgn = n.z >= 0.0 ? 1.0 : -1.0;
    float a   = -1.0 / (sgn + n.z);
    float c   = n.x * n.y * a;
    float3 t = float3(1.0 + sgn * n.x * n.x * a, sgn * c, -sgn * n.x);
    float3 b = float3(c, sgn + n.y * n.y * a, -n.y);

    float env_texel = 4.0 * kPI / (6.0 * float(pc.env_resolution) * float(pc.env_resolution));

    float3 irradiance = 0.0.xxx;

    const uint kSampleCount = 256u;
    for (uint i = 0u; i < kSampleCount; ++i)
    {
        float2 xi = hammersley(i, kSampleCount);

        // Malley's method: cosine-weighted hemisphere, pdf = cos_theta / kPI
        float phi       = 2.0 * kPI * xi.y;
        float cos_theta = sqrt(1.0 - xi.x);
        float sin_theta = sqrt(xi.x);

        float3 ls = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
        float3 ws = ls.x * t + ls.y * b + ls.z * n;

        float sample_solid = kPI / (float(kSampleCount) * max(cos_theta, 1e-4));
        float mip = max(0.5 * log2(sample_solid / env_texel), 0.0);

        irradiance += i_cubemap.SampleLevel(i_cubemap_sampler, ws, mip).rgb;
    }

    irradiance /= float(kSampleCount);
    o_convolution[global_id.xyz] = float4(irradiance, 1.0);
}
#endif

#ifdef PREFILTER_MAIN
[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    int3 image_size = pc.resolution.xxx;
    if (any(global_id.xy >= uint2(image_size.xy)))
        return;

    uint face = global_id.z;

    float2 pixel = float2(global_id.xy);
    float2 uv = to_ndc(pixel, float2(image_size.xy));

    float3 view  = normalize(kFaceForward[face] + uv.x * kFaceRight[face] + uv.y * kFaceUp[face]);

    float alpha = pc.roughness * pc.roughness;
    float env_texel  = 4.0 * kPI / (6.0 * pc.env_resolution * pc.env_resolution);

    float3 nrm = view;

    float weight = 0.0;
    float3 prefilter_result = 0.0.xxx;

    const uint kSampleCount = 1024U;
    for(uint i = 0u; i < kSampleCount; ++i)
    {
        float2 xi = hammersley(i, kSampleCount);
        float3 hv = importance_sample_ggx(xi, nrm, pc.roughness);
        float3 lv = normalize(2.0 * dot(view, hv) * hv - view);

        float n_dot_l = max(dot(nrm, lv), 0.0);
        float n_dot_h = max(dot(nrm, hv), 0.0);
        float h_dot_v = max(dot(hv, view), 0.0);

        if(n_dot_l > 0.0)
        {
            weight += n_dot_l;

            float dst   = distribution_townbridge_reitz_ggx(n_dot_h, alpha);
            float pdf = (dst * n_dot_h / (4.0 * h_dot_v)) + 0.0001;

            float env_sample = 1.0 / (float(kSampleCount) * pdf + 0.0001);
            float mip_level = pc.roughness == 0.0 ? 0.0 : 0.5 * log2(env_sample / env_texel);

            prefilter_result += i_cubemap.SampleLevel(i_cubemap_sampler, lv, mip_level).rgb * n_dot_l;
        }
    }

    prefilter_result = prefilter_result / max(weight, 1e-4);
    o_prefilter[global_id.xyz] = float4(prefilter_result, 1.0);
}
#endif
