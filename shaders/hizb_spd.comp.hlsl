#define A_GPU 1
#define A_HLSL 1
#define SPD_LINEAR_SAMPLER 1 // SpdLoadSourceImage() returns an already reduced value of a 2x2 block (== HiZB mip 0)

#define AU1  uint
#define AU2  uint2
#define ASU2 int2
#define AF4  float // we only need a single channel, so keep the LDS and the wave shuffles scalar

#define kReduceNeutral 1.0F

AU2 ARmpRed8x8(AU1 a)
{
    return AU2((a & 1u) | (((a >> 3u) & 3u) << 1u), ((a >> 1u) & 3u) | (((a >> 5u) & 1u) << 2u));
}

[[vk::combinedImageSampler]] Texture2D    src_depth:         register(t0);
[[vk::combinedImageSampler]] SamplerState src_depth_sampler: register(s0);

globallycoherent RWStructuredBuffer<uint> spd_global_counter: register(u1);

// Only mip 5 is read back (by the last workgroup), so it is the only one that has to be coherent
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_0:  register(u2);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_1:  register(u3);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_2:  register(u4);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_3:  register(u5);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_4:  register(u6);
[[vk::image_format("r32f")]] globallycoherent RWTexture2D<float> hiz_mip_5:  register(u7);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_6:  register(u8);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_7:  register(u9);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_8:  register(u10);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_9:  register(u11);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_10: register(u12);
[[vk::image_format("r32f")]]                  RWTexture2D<float> hiz_mip_11: register(u13);

struct PushConstants
{
    uint2 src_size;
    uint2 hiz_size;
    uint  mips;
    uint  workgroups;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

groupshared AF4 g_spd_intermediate[16][16];
groupshared AU1 g_spd_counter;

uint2 mip_size(uint mip)
{
    return max(pc.hiz_size >> mip, uint2(1, 1));
}

// `p` is the top left texel of a 2x2 block of the (virtual) 2x hiz_size source, so p / 2 is the HiZB mip 0 texel
AF4 SpdLoadSourceImage(ASU2 p, AU1 slice)
{
    const uint2 texel = uint2(p) >> 1u;
    if (any(texel >= pc.hiz_size))
    {
        return kReduceNeutral;
    }

    // Depth texel range covered by this HiZB texel: [lo, hi]
    const uint2 lo = (texel * pc.src_size) / pc.hiz_size;
    const uint2 hi = min(((texel + 1u) * pc.src_size + pc.hiz_size - 1u) / pc.hiz_size, pc.src_size) - 1u;

    float result = kReduceNeutral;
    for (uint y = lo.y; y <= hi.y; ++y)
    {
        for (uint x = lo.x; x <= hi.x; ++x)
        {
            result = min(result, src_depth.Load(int3(x, y, 0)).x);
        }
    }

    return result;
}

AF4 SpdLoad(ASU2 p, AU1 slice)
{
    if (any(uint2(p) >= mip_size(5)))
    {
        return kReduceNeutral;
    }

    return hiz_mip_5[uint2(p)];
}

void SpdStore(ASU2 pix, AF4 value, AU1 mip, AU1 slice)
{
    if (any(uint2(pix) >= mip_size(mip)))
    {
        return;
    }

    const uint2 p = uint2(pix);
    switch (mip)
    {
    case 0 :  hiz_mip_0[p]  = value; break;
    case 1 :  hiz_mip_1[p]  = value; break;
    case 2 :  hiz_mip_2[p]  = value; break;
    case 3 :  hiz_mip_3[p]  = value; break;
    case 4 :  hiz_mip_4[p]  = value; break;
    case 5 :  hiz_mip_5[p]  = value; break;
    case 6 :  hiz_mip_6[p]  = value; break;
    case 7 :  hiz_mip_7[p]  = value; break;
    case 8 :  hiz_mip_8[p]  = value; break;
    case 9 :  hiz_mip_9[p]  = value; break;
    case 10 : hiz_mip_10[p] = value; break;
    case 11 : hiz_mip_11[p] = value; break;
    default : break;
    }
}

AU1 SpdGetAtomicCounter()
{
    DeviceMemoryBarrier();
    return g_spd_counter;
}

void SpdIncreaseAtomicCounter(AU1 slice)
{
    DeviceMemoryBarrier();
    InterlockedAdd(spd_global_counter[0], 1, g_spd_counter);
}

void SpdResetAtomicCounter(AU1 slice)
{
    spd_global_counter[0] = 0;
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y)
{
    return g_spd_intermediate[x][y];
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
    g_spd_intermediate[x][y] = value;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
    return min(min(v0, v1), min(v2, v3));
}

#include "include/shaders/spd.hlsl.h"

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 group_id: SV_GroupID, uint local_index: SV_GroupIndex)
{
    SpdDownsample(AU2(group_id.xy), AU1(local_index), AU1(pc.mips), AU1(pc.workgroups), AU1(0));
}
