#version 450

in VS_IN {
    layout (location = 0) in vec2 uv;
    layout (location = 1) in vec3 normal;
    layout (location = 2) in vec3 tangent;
    layout (location = 3) in vec3 bitangent;
    layout (location = 4) in vec4 world_pos;
} vs_in;

layout (push_constant) uniform constants
{
    mat4 vp;
    vec4 sun_pos;
    vec4 view_pos;
} pc;

layout (location = 0) out vec4 o_frag_color;

void main()
{
    const float diffuse_contribution = 1.0F;

    vec3 diffuse = vec3(max(dot(pc.sun_pos.xyz - vs_in.world_pos.xyz, vs_in.normal), 0.0) * diffuse_contribution);
    o_frag_color = vec4(vec3(1.0F - diffuse_contribution) + diffuse, 1.0F);
}
