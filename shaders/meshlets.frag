#version 460
#extension GL_GOOGLE_include_directive: require

#include "include/shaders/constants.h"

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) in vec3 normal;
    layout (location = 2) in vec3 tangent;
    layout (location = 3) in vec3 bitangent;
    layout (location = 4) in vec4 world_pos;
#if VISUALIZE_MESHLETS
    layout (location = 5) flat in uint meshlet_id;
#endif
} vs_in;

layout (location = 0) out vec4 o_frag_color;


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

void main()
{
#if VISUALIZE_MESHLETS
    o_frag_color = vec4(meshlet_color(vs_in.meshlet_id), 1.0F);
#else
    o_frag_color = vec4(vs_in.normal, 1.0F);
#endif
}
