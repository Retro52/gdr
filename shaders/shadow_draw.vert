#version 460

#extension GL_GOOGLE_include_directive: require

#include "utils/math.glsl"
#include "include/shaders/types.h"
#include "include/shaders/bindings/shadow_draw.h"

layout (binding = kVertexBinding,   set = 0) readonly buffer Vertices             { Vertex vertices[]; };
layout (binding = kInstanceBinding, scalar)  readonly buffer MeshInstances        { MeshInstance mesh_instances[]; };
layout (binding = kMaterialBinding, set = 0) readonly buffer MeshMaterials        { MeshMaterial mesh_materials[]; };
layout (binding = kDrawBinding,     set = 0) readonly buffer DrawIndexedIndirects { DrawIndexedIndirect draw_cmds[]; };

layout (push_constant) uniform constants
{
    ShadowDrawPushConstants pc;
};

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) out flat uint material_albedo;
    layout (location = 2) out flat float material_alpha;
} vs_out;

void main()
{
#ifdef PLATFORM_MVK
    uint cid = gl_BaseInstance;
#else
    uint cid = gl_DrawID;
#endif

    uint instance_id = draw_cmds[cid].instance_id;

    vs_out.uv = vec2(vertices[gl_VertexIndex].ux, vertices[gl_VertexIndex].uy);
    vs_out.material_albedo = mesh_materials[mesh_instances[instance_id].material_index].albedo_idx;
    vs_out.material_alpha  = mesh_materials[mesh_instances[instance_id].material_index].diffuse_factor.a;

    vec3 vpos = vec3(vertices[gl_VertexIndex].px, vertices[gl_VertexIndex].py, vertices[gl_VertexIndex].pz);
    gl_Position = pc.vp * vec4(transform_vec3(vpos, mesh_instances[instance_id].pos_and_scale, mesh_instances[instance_id].rotation_quat), 1.0F);
}
