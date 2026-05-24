#version 460

#extension GL_GOOGLE_include_directive: require

#include "common.glsl"
#include "include/shaders/types.h"

layout (binding = 2) readonly buffer Vertices
{
    Vertex vertices[];
};

layout (scalar, binding = 3) readonly buffer MeshInstances
{
    MeshInstance mesh_instances[];
};

layout (binding = 4) readonly buffer DrawIndexedIndirects
{
    DrawIndexedIndirect draw_cmds[];
};

layout (push_constant) uniform constants
{
    mat4 vp;
} pc;

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) out vec3 normal;
    layout (location = 2) out vec4 tangent;
    layout (location = 3) out vec3 world_pos;
    layout (location = 4) out flat uint material_id;
#if SHADERS_DEBUG
    layout (location = 5) out flat uint meshlet_id;
#endif
} vs_out;

void main()
{
    uint instance_id = draw_cmds[gl_DrawID].instance_id;

    vec3 vpos = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
    vec3 vnorm = vec3(vertices[gl_VertexIndex].nx, vertices[gl_VertexIndex].ny, vertices[gl_VertexIndex].nz);

    vs_out.uv = vec2(vertices[gl_VertexIndex].ux, vertices[gl_VertexIndex].uy);

    vs_out.normal = quat_rotate_vec3(vnorm, mesh_instances[instance_id].rotation_quat);

    vec4 tangent = vec4(vertices[gl_VertexIndex].tx, vertices[gl_VertexIndex].ty, vertices[gl_VertexIndex].tz, vertices[gl_VertexIndex].tw);
    vs_out.tangent = vec4(quat_rotate_vec3(tangent.xyz, mesh_instances[instance_id].rotation_quat), tangent.w);

    vs_out.material_id = mesh_instances[instance_id].material_index;

#if SHADERS_DEBUG
    vs_out.meshlet_id = gl_VertexIndex;
#endif

    vs_out.world_pos = transform_vec3(vpos, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat);
    gl_Position = pc.vp * vec4(vs_out.world_pos, 1.0F);
}
