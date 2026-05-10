#version 460

#extension GL_GOOGLE_include_directive: require

#include "common.glsl"
#include "include/shaders/types.h"

layout (binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

layout (scalar, binding = 1) readonly buffer MeshInstances
{
    MeshInstance mesh_instances[];
};

layout (binding = 2) readonly buffer DrawIndexedIndirects
{
    DrawIndexedIndirect draw_cmds[];
};

layout (push_constant) uniform constants
{
    mat4 vp;
} pc;

out VS_OUT {
    layout (location = 0) out vec3 normal;
#if VISUALIZE_MESHLETS
    layout (location = 1) out flat uint meshlet_id;
#endif
} vs_out;

void main()
{
    vec3 vpos = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
    vec3 vnorm = vec3(vertices[gl_VertexIndex].nx, vertices[gl_VertexIndex].ny, vertices[gl_VertexIndex].nz);

    vs_out.normal = vnorm;
#if 0
    vs_out.uv = vec2(v.ux, v.uy);
    vs_out.tangent = vec3(v.tx, v.ty, v.tz);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);
#endif

#if VISUALIZE_MESHLETS
    vs_out.meshlet_id = gl_VertexIndex;
#endif

    uint instance_id = draw_cmds[gl_DrawID].instance_id;
    vec4 world_pos = vec4(transform_vec3(vpos, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat), 1.0);

    gl_Position = pc.vp * world_pos;
}
