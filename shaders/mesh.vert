#version 450

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float ux, uy;
    float tx, ty, tz;
};

layout (binding = 0) readonly buffer Vertices
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

    vs_out.uv = vec2(v.ux, v.uy);
    vs_out.normal = vec3(v.nx, v.ny, v.nz);
    vs_out.tangent = vec3(v.tx, v.ty, v.tz);
    vs_out.world_pos = vec4(v.px, v.py, v.pz, 1.0F);
    vs_out.bitangent = cross(vs_out.tangent, vs_out.normal);

    vs_out.sun_pos = pc.sun_pos;
    gl_Position = pc.vp * vs_out.world_pos;
}
