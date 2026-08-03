#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

#include "include/shaders/types.h"
#include "include/shaders/bindings/shadow_draw.h"

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) flat in uint material_albedo;
    layout (location = 2) flat in float material_alpha;
} vs_in;

layout (binding = kTextureBinding,  set = 0) uniform sampler textures_sampler;
layout (binding = 0, set = 1) uniform texture2D textures[];

#define TEXTURE2D(id, uv) texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv)

void main()
{
    float a = vs_in.material_albedo > 0 ? TEXTURE2D(vs_in.material_albedo, vs_in.uv).a * vs_in.material_alpha : vs_in.material_alpha;
    if (a < 0.5F)
        discard;
}
