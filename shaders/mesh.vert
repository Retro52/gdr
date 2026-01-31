#version 460

#extension GL_GOOGLE_include_directive: require

#include "types.glsl"
#include "common.glsl"

layout (binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

layout (binding = 1) readonly buffer MeshesTransforms
{
    MeshTransform meshes_transforms[];
};

layout (binding = 2) readonly buffer DrawIndexedIndirects
{
    DrawIndexedIndirect draw_cmds[];
};

// FIXME: most of this data is temporary and may exceed the hardware limits
// TODO: move out to the separate buffer
layout (push_constant) uniform constants
{
    mat4 vp;
    mat4 view;
    uint padding;
} pc;

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) out vec3 normal;
    layout (location = 2) out vec3 tangent;
    layout (location = 3) out vec3 bitangent;
    layout (location = 4) out vec4 world_pos;
#if VISUALIZE_MESHLETS
    layout (location = 5) out flat uint meshlet_id;
#endif
} vs_out;

void main()
{
    Vertex v = vertices[gl_VertexIndex];

    vec3 local_pos = vec3(v.px, v.py, v.pz);
    uint mesh_id = draw_cmds[gl_DrawID].mesh_id;

    vs_out.world_pos = vec4(transform_vec3(local_pos, meshes_transforms[mesh_id].pos_and_scale, meshes_transforms[mesh_id].rotation_quat), 1.0);
    vs_out.normal = vec3(v.nx, v.ny, v.nz);

#if 0
    vs_out.uv = vec2(v.ux, v.uy);
    vs_out.tangent = vec3(v.tx, v.ty, v.tz);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);
#endif

#if VISUALIZE_MESHLETS
    vs_out.meshlet_id = gl_VertexIndex;
#endif

    gl_Position = pc.vp * vs_out.world_pos;
}
