#version 450

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
};

layout (binding = 0) buffer Vertices
{
    Vertex vertices[];
};

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
    Vertex v = vertices[gl_VertexIndex];

    vs_out.uv = v.uv;
    vs_out.normal = v.normal;
    vs_out.tangent = v.tangent;
    vs_out.sun_pos = pc.sun_pos;
    vs_out.world_pos = vec4(v.position, 1.0F);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);

    gl_Position = pc.vp * vs_out.world_pos;
}
