#version 450

#extension GL_GOOGLE_include_directive: require

#include "types.glsl"
#include "common.glsl"

layout (binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

// FIXME: most of this data is temporary and may exceed the hardware limits
// TODO: move out to the separate buffer
layout (push_constant) uniform constants
{
    mat4 vp;
    mat4 view;
    vec4 pos_and_scale; // xyz - position, w - uniform scale
    vec4 rotation_quat; // quaternion representing object position
} pc;

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) out vec3 normal;
    layout (location = 2) out vec3 tangent;
    layout (location = 3) out vec3 bitangent;
    layout (location = 4) out vec4 world_pos;
#if VISUALIZE_MESHLETS
    layout (location = 5) out flat uint meshlet_id;
    layout (location = 6) out flat uint triangle_id;
#endif
} vs_out;

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    vec3 local_pos = vec3(v.px, v.py, v.pz);
    vs_out.world_pos = vec4(transform_vec3(local_pos, pc.pos_and_scale, pc.rotation_quat), 1.0);
    vs_out.normal = vec3(v.nx, v.ny, v.nz);

#if 0
    vs_out.uv = vec2(v.ux, v.uy);
    vs_out.tangent = vec3(v.tx, v.ty, v.tz);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);
#endif

#if VISUALIZE_MESHLETS
    vs_out.meshlet_id = 0x225;
    vs_out.triangle_id = gl_VertexIndex;
#endif

    gl_Position = pc.vp * vs_out.world_pos;
}
