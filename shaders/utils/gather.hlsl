// https://registry.khronos.org/OpenGL-Refpages/gl4/html/textureGather.xhtml
// https://registry.khronos.org/OpenGL-Refpages/gl4/html/textureLod.xhtml
float4 textureGatherLod(Texture2D tex, SamplerState tex_sampler, float2 uv, float lod, int comp)
{
    float width, height, levels;
    tex.GetDimensions(uint(lod), width, height, levels);

    float2 size = float2(width, height);
    float2 tx_size = 1.0 / size;

    float2 f = uv * size - 0.5;
    float2 tx_base = floor(f);

    float2 base = (tx_base + 0.5) * tx_size;

    float s00 = tex.SampleLevel(tex_sampler, base + tx_size * float2(0.0, 0.0), lod)[comp];
    float s10 = tex.SampleLevel(tex_sampler, base + tx_size * float2(1.0, 0.0), lod)[comp];
    float s01 = tex.SampleLevel(tex_sampler, base + tx_size * float2(0.0, 1.0), lod)[comp];
    float s11 = tex.SampleLevel(tex_sampler, base + tx_size * float2(1.0, 1.0), lod)[comp];

    return float4(s01, s11, s10, s00);
}

float textureGatherMin(Texture2D tex, SamplerState tex_sampler, float2 uv, int comp)
{
    float4 gresult = tex.GatherRed(tex_sampler, uv);
    return min(gresult.r, min(gresult.g, min(gresult.b, gresult.a)));
}

float textureGatherLodMin(Texture2D tex, SamplerState tex_sampler, float2 uv, float lod, int comp)
{
    float4 gresult = textureGatherLod(tex, tex_sampler, uv, lod, comp);
    return min(gresult.r, min(gresult.g, min(gresult.b, gresult.a)));
}
