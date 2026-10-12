#include "include/shaders/types.hlsl"

// TODO(dx12): binding constants (include/shaders/bindings/shadow_draw.h) can't be pasted into register(), keep in sync
SamplerState textures_sampler: register(s2);
Texture2D    textures[]:       register(t0, space1);

#define TEXTURE2D(id, uv) textures[NonUniformResourceIndex(id)].Sample(textures_sampler, uv)

struct Input
{
    float4 position: SV_Position;
    [[vk::location(0)]] float2 uv: TEXCOORD0;
    [[vk::location(1)]] nointerpolation uint material_albedo: TEXCOORD1;
    [[vk::location(2)]] nointerpolation float material_alpha: TEXCOORD2;
};

[shader("pixel")]
void main(Input vs_in)
{
    float a = vs_in.material_albedo > 0 ? TEXTURE2D(vs_in.material_albedo, vs_in.uv).a * vs_in.material_alpha : vs_in.material_alpha;
    if (a < 0.5F)
        discard;
}
