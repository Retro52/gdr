#version 460
#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier: require

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

vec3 meshlet_color(uint id)
{
    return uint_color(lowbias32(id));
}

// https://www.shadertoy.com/view/4tXcWr
vec4 from_linear(vec4 rgb_linear)
{
    bvec4 cutoff = lessThan(rgb_linear, vec4(0.0031308));
    vec4 higher = vec4(1.055)*pow(rgb_linear, vec4(1.0/2.4)) - vec4(0.055);
    vec4 lower = rgb_linear * vec4(12.92);

    return mix(higher, lower, cutoff);
}

const uint draw_debug_shaded           = 0;
const uint draw_debug_diffuse_factor   = 1;
const uint draw_debug_specular_factor  = 2;
const uint draw_debug_albedo_texture   = 3;
const uint draw_debug_normal_texture   = 4;
const uint draw_debug_specular_texture = 5;
const uint draw_debug_uv               = 6;
const uint draw_debug_normal           = 7;
const uint draw_debug_tangent          = 8;
const uint draw_debug_world_pos        = 9;
const uint draw_debug_mat_idx          = 10;
const uint draw_debug_mat_type        = 11;
const uint draw_debug_mlt_idx          = 12;
const uint draw_debug_white_lit        = 13;
const uint draw_debug_white_diffuse    = 14;
const uint draw_debug_white_specular   = 15;

void main()
{
#if SHADERS_DEBUG
    if (pc.debug_mode != draw_debug_shaded)
    {
        if (pc.debug_mode == draw_debug_diffuse_factor)   {o_frag_color = materials[vs_in.material_id].diffuse_factor;                             return;}
        if (pc.debug_mode == draw_debug_specular_factor)  {o_frag_color = materials[vs_in.material_id].specular_factor;                            return;}
        if (pc.debug_mode == draw_debug_albedo_texture)   {o_frag_color = TEXTURE2D(materials[vs_in.material_id].albedo_idx, vs_in.uv);            return;}
        if (pc.debug_mode == draw_debug_normal_texture)   {o_frag_color = TEXTURE2D(materials[vs_in.material_id].normal_idx, vs_in.uv);            return;}
        if (pc.debug_mode == draw_debug_specular_texture) {o_frag_color = TEXTURE2D(materials[vs_in.material_id].specular_idx, vs_in.uv);          return;}
        if (pc.debug_mode == draw_debug_normal)           {o_frag_color = vec4(vs_in.normal, 1.0F);                                                return;}
        if (pc.debug_mode == draw_debug_uv)               {o_frag_color = vec4(vs_in.uv, 0.0F, 1.0F);                                              return;}
        if (pc.debug_mode == draw_debug_tangent)          {o_frag_color = vec4(normalize(vs_in.tangent.xyz), 1.0F);                                return;}
        if (pc.debug_mode == draw_debug_world_pos)        {o_frag_color = vec4(normalize(vs_in.world_pos), 1.0F);                                  return;}
        if (pc.debug_mode == draw_debug_mat_idx)          {o_frag_color = vec4(meshlet_color(vs_in.material_id), 1.0F);                            return;}
        if (pc.debug_mode == draw_debug_mat_type)         {o_frag_color = vec4(meshlet_color(materials[vs_in.material_id].material_type), 1.0F);   return;}
        if (pc.debug_mode == draw_debug_mlt_idx)          {o_frag_color = vec4(meshlet_color(vs_in.meshlet_id), 1.0F);                             return;}
    }
#endif
    uint albedo_idx = materials[vs_in.material_id].albedo_idx;
    uint normal_idx = materials[vs_in.material_id].normal_idx;
    uint specular_idx = materials[vs_in.material_id].specular_idx;
    o_frag_color = albedo_idx > 0 ? TEXTURE2D(albedo_idx, vs_in.uv) * materials[vs_in.material_id].diffuse_factor : materials[vs_in.material_id].diffuse_factor;

    if (o_frag_color.a < 0.5)
        discard;

    vec3 tex_normal = vec3(0, 0, 1);
    if (normal_idx > 0)
        tex_normal = TEXTURE2D(normal_idx, vs_in.uv).rgb * 2 - 1;

    vec3 normal = normalize(vs_in.normal);
    vec3 tangent = normalize(vs_in.tangent.xyz);

    vec3 bitangent = cross(normal, tangent) * vs_in.tangent.w;
    vec3 nrm = normalize(tex_normal.r * tangent.xyz + tex_normal.g * bitangent + tex_normal.b * normal);

    vec4 specular_factor = specular_idx > 0 ? TEXTURE2D(specular_idx, vs_in.uv) * materials[vs_in.material_id].specular_factor : materials[vs_in.material_id].specular_factor;

    vec3 view = normalize(pc.camera_pos - vs_in.world_pos);
    vec3 halfw = normalize(pc.sun_direction + view);

    float diffuse = max(dot(nrm, pc.sun_direction), 0.0F);
    float shininess = max(specular_factor.w * 128.0, 1.0);
    float specular = specular_factor.w > 0.0F ? pow(max(dot(nrm, halfw), 0.0), shininess) : 0.0F;

    vec3 ambient = vec3(1.0F);
    vec3 specular_color = specular_factor.rgb * specular;

    const float kAmbiance = 0.5F;

#if SHADERS_DEBUG
    vec3 color = o_frag_color.xyz * pc.sun_color;
    if (pc.debug_mode == draw_debug_white_lit)      o_frag_color = vec4(ambient * kAmbiance + vec3(diffuse) * (1.0F - kAmbiance) + vec3(specular), 1.0F);
    if (pc.debug_mode == draw_debug_white_diffuse)  o_frag_color = vec4(vec3(diffuse), 1.0F);
    if (pc.debug_mode == draw_debug_white_specular) o_frag_color = vec4(specular_color, 1.0F);
    if (pc.debug_mode == draw_debug_shaded)         o_frag_color = vec4(ambient * kAmbiance * color + diffuse * color * (1.0F - kAmbiance) + specular_color, 1.0F);
#else
    vec3 color = o_frag_color.xyz * pc.sun_color;
    o_frag_color = vec4(ambient * kAmbiance * color + diffuse * color * (1.0F - kAmbiance) + specular_color, 1.0F);
#endif

    o_frag_color = from_linear(o_frag_color);
}
