#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

#include "include/shaders/types.h"

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) flat in uint instance_id;
#ifndef FOR_MESH_PIPELINE
    layout (location = 2) flat in uint base_index;
#endif
} vs_in;

layout (location = 0) out uvec2 o_indices;

layout (binding = 1, set = 0) readonly buffer Materials { MeshMaterial materials[]; };
layout (binding = 2, set = 0) uniform sampler textures_sampler;
layout (binding = 6, scalar)  readonly buffer MeshInstances   { MeshInstance mesh_instances[]; };

layout (binding = 0, set = 1) uniform texture2D textures[];

#define TEXTURE2D(id, uv) texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv)

void main()
{
#ifdef ENABLE_ALPHA_TESTING
    uint albedo_idx = uint(materials[mesh_instances[vs_in.instance_id].material_index].albedo_idx);
    vec4 frag_color = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) : vec4(1.0F);
    frag_color *= materials[mesh_instances[vs_in.instance_id].material_index].diffuse_factor;

    if (frag_color.a < 0.5F)
        discard;
#endif

#ifdef FOR_MESH_PIPELINE
    o_indices = uvec2(vs_in.instance_id, gl_PrimitiveID);
#else
    o_indices = uvec2(vs_in.instance_id, vs_in.base_index + 3 * gl_PrimitiveID);
#endif
}
