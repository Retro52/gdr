#version 460

#extension GL_GOOGLE_include_directive: require

#include "utils/math.glsl"
#include "include/shaders/types.h"

layout (binding = 0, set = 0) readonly buffer Vertices             { Vertex vertices[]; };
layout (binding = 6, scalar)  readonly buffer MeshInstances        { MeshInstance mesh_instances[]; };
layout (binding = 7, set = 0) readonly buffer DrawIndexedIndirects { DrawIndexedIndirect draw_cmds[]; };

layout (push_constant) uniform constants
{
    DrawPushConstants pc;
};

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) flat out uint instance_id;
    layout (location = 2) flat out uint base_index;
} vs_out;

void main()
{
#ifdef PLATFORM_MVK
    uint cid = gl_BaseInstance;
#else
    uint cid = gl_DrawID;
#endif

    vs_out.base_index = draw_cmds[cid].first_index;
    vs_out.instance_id = draw_cmds[cid].instance_id;
    vs_out.uv = vec2(vertices[gl_VertexIndex].ux, vertices[gl_VertexIndex].uy);

    vec3 vpos = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
    gl_Position = pc.vp * vec4(transform_vec3(vpos, mesh_instances[vs_out.instance_id].pos_and_scale, mesh_instances[vs_out.instance_id].rotation_quat), 1.0F);
}
