#version 450
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
    layout (location = 6) flat in uint triangle_id;
#endif
} vs_in;

layout (push_constant) uniform constants
{
    mat4 vp;
    vec4 sun_pos;
    vec4 view_pos;
    vec4 view_dir;
} pc;

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

vec3 meshlet_color(uint id, uint tris)
{
#if VISUALIZE_MESHLET_TRIANGLES
    float kTrisColFactor = 0.15F;
#else
    float kTrisColFactor = 0.0F;
#endif
    return (1.0F - kTrisColFactor) * uint_color(lowbias32(id)) + kTrisColFactor * uint_color(lowbias32(tris));
}

void main()
{
#if VISUALIZE_MESHLETS
    o_frag_color = vec4(meshlet_color(vs_in.meshlet_id, vs_in.triangle_id), 1.0F);
#else
    const float diffuse_contribution = 1.0F;

    vec3 diffuse = vec3(max(dot(pc.sun_pos.xyz - vs_in.world_pos.xyz, vs_in.normal), 0.0) * diffuse_contribution);
    o_frag_color = vec4(vec3(1.0F - diffuse_contribution) + diffuse, 1.0F);
#endif
}
