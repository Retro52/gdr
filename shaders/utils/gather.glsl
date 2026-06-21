// https://registry.khronos.org/OpenGL-Refpages/gl4/html/textureGather.xhtml
// https://registry.khronos.org/OpenGL-Refpages/gl4/html/textureLod.xhtml
vec4 textureGatherLod(sampler2D tex, vec2 uv, float lod, int comp)
{
    vec2 size = vec2(textureSize(tex, int(lod)));
    vec2 tx_size = 1.0 / size;

    vec2 f = uv * size - 0.5;
    vec2 tx_base = floor(f);

    vec2 base = (tx_base + 0.5) * tx_size;

    float s00 = textureLod(tex, base + tx_size * vec2(0.0, 0.0), lod)[comp];
    float s10 = textureLod(tex, base + tx_size * vec2(1.0, 0.0), lod)[comp];
    float s01 = textureLod(tex, base + tx_size * vec2(0.0, 1.0), lod)[comp];
    float s11 = textureLod(tex, base + tx_size * vec2(1.0, 1.0), lod)[comp];

    return vec4(s01, s11, s10, s00);
}

float textureGatherMin(sampler2D tex, vec2 uv, int comp)
{
    vec4 gresult = textureGather(tex, uv);
    return min(gresult.r, min(gresult.g, min(gresult.b, gresult.a)));
}

float textureGatherLodMin(sampler2D tex, vec2 uv, float lod, int comp)
{
    vec4 gresult = textureGatherLod(tex, uv, lod, comp);
    return min(gresult.r, min(gresult.g, min(gresult.b, gresult.a)));
}
