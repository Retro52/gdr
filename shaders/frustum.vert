#version 450

layout (push_constant) uniform pc {
    mat4 renderer_vp;
    mat4 target_inv_vp;
};

// NDC corners - reverse-Z: near=1, far=0
const vec4 corners[8] = vec4[](
vec4(-1, -1, 1, 1), // 0: near bottom-left
vec4(1, -1, 1, 1),  // 1: near bottom-right
vec4(-1, 1, 1, 1),  // 2: near top-left
vec4(1, 1, 1, 1),   // 3: near top-right
vec4(-1, -1, 0, 1), // 4: far bottom-left
vec4(1, -1, 0, 1),  // 5: far bottom-right
vec4(-1, 1, 0, 1),  // 6: far top-left
vec4(1, 1, 0, 1)    // 7: far top-right
);

const int indices[24] = int[](
// near quad
0, 1,
1, 3,
3, 2,
2, 0,
// far quad
4, 5,
5, 7,
7, 6,
6, 4,
// connections
0, 4,
1, 5,
2, 6,
3, 7
);

void main()
{
    vec4 ndc = corners[indices[gl_VertexIndex]];

    vec4 world = target_inv_vp * ndc;
    world /= world.w;

    gl_Position = renderer_vp * world;
}
