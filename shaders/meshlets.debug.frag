#version 450

in VS_IN {
    layout (location = 5) flat in uint meshlet_id;
    layout (location = 6) flat in uint triangle_id;
} vs_in;

layout (location = 0) out vec4 o_frag_color;

// https://stackoverflow.com/questions/23319289/is-there-a-good-glsl-hash-function
uint lowbias32(uint x)
{
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

vec3 meshlet_color(uint id, uint tris)
{
    return 0.85 * uint_color(lowbias32(id)) + 0.15 * uint_color(lowbias32(tris));
}

void main()
{
    o_frag_color = vec4(meshlet_color(vs_in.meshlet_id, vs_in.triangle_id), 1.0F);
}
