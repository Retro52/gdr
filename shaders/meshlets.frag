#version 460
#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

#include "include/shaders/types.h"

in VS_IN {
    layout (location = 0) in vec3 normal;
    layout (location = 1) in vec2 uv;
    layout (location = 2) in flat uint material_id;
#if VISUALIZE_MESHLETS
    layout (location = 3) flat in uint meshlet_id;
#endif
} vs_in;

layout (location = 0) out vec4 o_frag_color;

layout (binding = 0, set = 0) readonly buffer Materials
{
    MeshMaterial materials[];
};

layout (binding = 1, set = 0) uniform sampler textures_sampler;
layout (binding = 0, set = 1) uniform texture2D textures[];

#define TEXTURE2D(id, uv) texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv)

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

vec3 uint_color(uint num)
{
    return vec3(float(num & 0xFF) / 255.0F,
    float((num >> 8) & 0xFF) / 255.0F,
    float((num >> 16) & 0xFF) / 255.0F
    );
}

vec3 meshlet_color(uint id)
{
    return uint_color(lowbias32(id));
}

// https://www.shadertoy.com/view/4tXcWr
vec4 from_linear(vec4 rgb_linear)
{
    bvec4 cutoff = lessThan(rgb_linear, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(rgb_linear, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = rgb_linear * vec4(12.92);

    return mix(higher, lower, cutoff);
}

void main()
{
    const vec3 sun_dir = vec3(-1.0F);

#if VISUALIZE_MESHLETS
    o_frag_color = vec4(meshlet_color(vs_in.meshlet_id), 1.0F);
#else
#if 0
    o_frag_color = vec4(mix(vec3(dot(vs_in.normal, sun_dir)), vec3(0.5F), 0.5F), 1.0F);
#else
    uint albedo_idx = materials[vs_in.material_id].albedo_idx;
    o_frag_color = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) : vec4(0);

    if (o_frag_color.a < 0.5)
        discard;

    o_frag_color = o_frag_color * 0.5F + o_frag_color * max(dot(vs_in.normal, sun_dir), 0.0F) * 0.5F;
    o_frag_color = from_linear(o_frag_color);
#endif
#endif
}
