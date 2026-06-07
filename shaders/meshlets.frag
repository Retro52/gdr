#version 460
#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

#include "utils/pbr.glsl"
#include "utils/post.glsl"
#include "include/shaders/types.h"

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) in vec3 normal;
    layout (location = 2) in vec4 tangent;
    layout (location = 3) in vec3 world_pos;
    layout (location = 4) flat in uint material_id;
#if SHADERS_DEBUG
    layout (location = 5) flat in uint meshlet_id;
#endif
} vs_in;

layout (location = 0) out vec4 o_frag_color;

layout (binding = 0, set = 0) readonly buffer Materials
{
    MeshMaterial materials[];
};

layout (binding = 1, set = 0) uniform sampler textures_sampler;
layout (binding = 0, set = 1) uniform texture2D textures[];

layout (scalar, push_constant) uniform constants
{
    mat4 vp;
    vec3 sun_direction;
    vec3 sun_color;
    vec3 camera_pos;
    uint debug_mode;
} pc;


#define TEXTURE2D(id, uv) texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv)
#define DTEXTURE2D(id, uv) (id > 0 ? texture(sampler2D(textures[nonuniformEXT(id)], textures_sampler), uv) : vec4(1.0F, 0.25F, 0.95F, 1.0F))

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

vec3 hash_color(uint id)
{
    return uint_color(lowbias32(id));
}

vec4 mr_from_sg(vec4 spec_gloss_texture)
{
    vec3 spec = from_srgb(spec_gloss_texture.rgb);

    float gloss = spec_gloss_texture.a;
    float metal = max(spec.r, max(spec.g, spec.b));

    return vec4(0.0F, gloss, metal, 1.0F);
}

const uint draw_debug_shaded           = 0;
const uint draw_debug_diffuse_factor   = 1;
const uint draw_debug_mr_factor        = 2;
const uint draw_debug_metallic         = 3;
const uint draw_debug_roughness        = 4;
const uint draw_debug_albedo_texture   = 5;
const uint draw_debug_normal_texture   = 6;
const uint draw_debug_mr_texture       = 7;
const uint draw_debug_uv               = 8;
const uint draw_debug_normal           = 9;
const uint draw_debug_tangent          = 10;
const uint draw_debug_world_pos        = 11;
const uint draw_debug_mat_idx          = 12;
const uint draw_debug_mat_type         = 13;
const uint draw_debug_mlt_idx          = 14;
const uint draw_debug_white_lit        = 15;
const uint draw_debug_white_diffuse    = 16;
const uint draw_debug_white_specular   = 17;

void main()
{
    uint albedo_idx = materials[vs_in.material_id].albedo_idx;
    uint normal_idx = materials[vs_in.material_id].normal_idx;
    uint met_roughness_idx = materials[vs_in.material_id].met_roughness_idx;
    vec4 frag_col = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) * materials[vs_in.material_id].diffuse_factor : materials[vs_in.material_id].diffuse_factor;

    if (frag_col.a < 0.5)
        discard;

    vec3 albedo = frag_col.rgb;
    vec3 tex_normal = vec3(0, 0, 1);
    if (normal_idx > 0)
        tex_normal = TEXTURE2D(normal_idx, vs_in.uv).rgb * 2 - 1;

    vec3 normal = normalize(vs_in.normal);
    vec3 tangent = normalize(vs_in.tangent.xyz);

    vec3 bitangent = cross(normal, tangent) * vs_in.tangent.w;
    vec3 nrm = normal_idx > 0 ? normalize(tex_normal.r * tangent.xyz + tex_normal.g * bitangent + tex_normal.b * normal) : normal;

#if 1
    vec4 met_roughness_factor = vec4(1.0F);
    if (GET_BIT(materials[vs_in.material_id].material_flags, kMatGlossBit) == 1)
    {
        vec4 factor = materials[vs_in.material_id].met_roughness_factor;
        met_roughness_factor = met_roughness_idx > 0 ? mr_from_sg(TEXTURE2D(met_roughness_idx, vs_in.uv)) * factor : factor;
        met_roughness_factor.g = 1.0F - met_roughness_factor.g;
    }
    else
    {
        met_roughness_factor = met_roughness_idx > 0 ? TEXTURE2D(met_roughness_idx, vs_in.uv) * materials[vs_in.material_id].met_roughness_factor : materials[vs_in.material_id].met_roughness_factor;
    }
#else
    vec4 met_roughness_factor = met_roughness_idx > 0 ? TEXTURE2D(met_roughness_idx, vs_in.uv) * materials[vs_in.material_id].met_roughness_factor : materials[vs_in.material_id].met_roughness_factor;
#endif

    vec3 view = normalize(pc.camera_pos - vs_in.world_pos);
    vec3 halfw = normalize(pc.sun_direction + view);

    float metallic = met_roughness_factor.b;
    float roughness = max(met_roughness_factor.g, 0.045);

    float n_dot_v = max(dot(nrm, view), 0.0F);
    float n_dot_h = max(dot(nrm, halfw), 0.0F);
    float n_dot_l = max(dot(nrm, pc.sun_direction), 0.0F);

    float alpha = roughness * roughness;

    float geom = geometry_smith(n_dot_v, n_dot_l, alpha / 2);
    float ndf = distribution_townbridge_reitz_ggx(n_dot_h, alpha);

    float h_dot_v = max(dot(halfw, view), 0.0F);
    vec3 fresnel = fresnel_schlick(h_dot_v, mix(vec3(0.04F), albedo, metallic));

    vec3 ambient = vec3(0.2F);
    vec3 diffuse = (vec3(1.0F) - fresnel) * (1.0F - metallic);
    vec3 specular = pbr_specular(fresnel, ndf, geom, n_dot_v, n_dot_l);

    const float kIntensity = 5.0F;
    vec3 sun_color_hdr = pc.sun_color * kIntensity;
    vec3 color = (diffuse * albedo / kPI + specular) * sun_color_hdr * n_dot_l + ambient * albedo;

#if SHADERS_DEBUG
    if (pc.debug_mode != draw_debug_shaded)
    {
        if (pc.debug_mode == draw_debug_diffuse_factor)   {color = materials[vs_in.material_id].diffuse_factor.rgb;                          }
        if (pc.debug_mode == draw_debug_mr_factor)        {color = materials[vs_in.material_id].met_roughness_factor.rgb;                    }
        if (pc.debug_mode == draw_debug_metallic)         {color = (vec3(metallic));                                                         }
        if (pc.debug_mode == draw_debug_roughness)        {color = (vec3(roughness));                                                        }
        if (pc.debug_mode == draw_debug_albedo_texture)   {color = DTEXTURE2D(materials[vs_in.material_id].albedo_idx, vs_in.uv).rgb;        }
        if (pc.debug_mode == draw_debug_normal_texture)   {color = DTEXTURE2D(materials[vs_in.material_id].normal_idx, vs_in.uv).rgb;        }
        if (pc.debug_mode == draw_debug_mr_texture)       {color = DTEXTURE2D(materials[vs_in.material_id].met_roughness_idx, vs_in.uv).rgb; }
        if (pc.debug_mode == draw_debug_normal)           {color = nrm;                                                                      }
        if (pc.debug_mode == draw_debug_uv)               {color = vec3(vs_in.uv, 0.0F);                                                     }
        if (pc.debug_mode == draw_debug_tangent)          {color = tangent;                                                                  }
        if (pc.debug_mode == draw_debug_world_pos)        {color = normalize(vs_in.world_pos);                                               }
        if (pc.debug_mode == draw_debug_mat_idx)          {color = hash_color(vs_in.material_id);                                            }
        if (pc.debug_mode == draw_debug_mat_type)         {color = hash_color(materials[vs_in.material_id].material_flags);                  }
        if (pc.debug_mode == draw_debug_mlt_idx)          {color = hash_color(vs_in.meshlet_id);                                             }
        if (pc.debug_mode == draw_debug_white_lit)        {color = tonemap((diffuse / kPI + specular) * sun_color_hdr * n_dot_l + ambient);  }
        if (pc.debug_mode == draw_debug_white_diffuse)    {color = tonemap((diffuse / kPI) * sun_color_hdr * n_dot_l);                       }
        if (pc.debug_mode == draw_debug_white_specular)   {color = tonemap(specular * sun_color_hdr * n_dot_l);                              }
        o_frag_color = from_linear(vec4(color, 1.0F));
        return;
    }
#endif
    o_frag_color = from_linear(vec4(tonemap(color), 1.0F));
}
