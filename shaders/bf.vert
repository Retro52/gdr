#version 450

layout (location = 0) in vec3 v_position;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec2 v_uv;
layout (location = 3) in vec3 v_tangent;


layout (push_constant) uniform constants
{
    mat4 vp;
    vec4 sun_pos;
    vec4 view_pos;
} pc;

out VS_OUT {
    layout (location = 0) out vec2 uv;
    layout (location = 1) out vec3 normal;
    layout (location = 2) out vec3 tangent;
    layout (location = 3) out vec3 bitangent;
    layout (location = 4) out vec4 world_pos;
    layout (location = 5) out vec4 sun_pos;
} vs_out;

void main()
{
    vs_out.uv = v_uv;
    vs_out.normal = v_normal;
    vs_out.tangent = v_tangent;
    vs_out.sun_pos = pc.sun_pos;
    vs_out.world_pos = vec4(v_position, 1.0F);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);

    gl_Position = pc.vp * vs_out.world_pos;
}
