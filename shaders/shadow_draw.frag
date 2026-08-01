#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

#include "include/shaders/types.h"
#include "include/shaders/bindings/shadow_draw.h"

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) flat in uint material_id;
} vs_in;

layout (binding = kMaterialBinding, set = 0) readonly buffer Materials { MeshMaterial materials[]; };
layout (binding = kTextureBinding,  set = 0) uniform sampler textures_sampler;

layout (binding = 0, set = 1) uniform texture2D textures[];

#define TEXTURE2D(id, uv) texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv)

void main()
{
    uint albedo_idx = uint(materials[vs_in.material_id].albedo_idx);
    vec4 frag_color = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) : vec4(1.0F);
    frag_color *= materials[vs_in.material_id].diffuse_factor;

    if (frag_color.a < 0.5F)
        discard;
}
