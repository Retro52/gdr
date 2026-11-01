#include "include/shaders/types.hlsl.h"

struct PushConstants
{
    float4x4 renderer_vp;
    float4 color;
    uint mode; // 0 - edges; 1 - faces
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc: register(b0);

StructuredBuffer<FrameCullData> frame_cull: register(t0);

// NDC corners - reverse-Z: near=1, far=0
static const float4 corners[8] = {
    float4(-1, -1, 1, 1), // 0: near bottom-left
    float4(1, -1, 1, 1),  // 1: near bottom-right
    float4(-1, 1, 1, 1),  // 2: near top-left
    float4(1, 1, 1, 1),   // 3: near top-right
    float4(-1, -1, 0, 1), // 4: far bottom-left
    float4(1, -1, 0, 1),  // 5: far bottom-right
    float4(-1, 1, 0, 1),  // 6: far top-left
    float4(1, 1, 0, 1)    // 7: far top-right
};

static const int face_indices[36] = {
    0, 1, 3,  0, 3, 2, // near
    4, 7, 5,  4, 6, 7, // far
    0, 2, 6,  0, 6, 4, // left
    1, 5, 7,  1, 7, 3, // right
    0, 4, 5,  0, 5, 1, // bottom
    2, 3, 7,  2, 7, 6  // top
};

static const int edge_indices[24] = {
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
};

// HLSL has no inverse(), https://www.geometrictools.com/Documentation/LaplaceExpansionTheorem.pdf
float4x4 inverse(float4x4 m)
{
    float s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
    float s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
    float s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
    float s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
    float s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
    float s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

    float c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

    float inv_det = 1.0 / (s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);

    return float4x4(
         m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3, -m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3,  m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3, -m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3,
        -m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1,  m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1, -m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1,  m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1,
         m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0, -m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0,  m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0, -m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0,
        -m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0,  m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0, -m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0,  m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0
    ) * inv_det;
}

struct Input
{
    uint vertex_id: SV_VertexID;
};

struct Output
{
    float4 color: TEXCOORD0;
    float4 position: SV_Position;
};

[shader("vertex")]
Output main(Input input)
{
    float far = frame_cull[0].frustum[5];
    float near = frame_cull[0].frustum[4];

    float4x4 proj = float4x4(
        frame_cull[0].p00, 0.0, 0.0, 0.0,
        0.0, frame_cull[0].p11, 0.0, 0.0,
        0.0, 0.0, near / (far - near), (near * far) / (far - near),
        0.0, 0.0, -1.0, 0.0
    );

    int idx = pc.mode == 0 ? edge_indices[input.vertex_id] : face_indices[input.vertex_id];

    float4 ndc = corners[idx];
    float4 world = mul(inverse(mul(proj, frame_cull[0].view)), ndc);
    world /= world.w;

    Output result;
    result.color = pc.color;
    result.position = mul(pc.renderer_vp, world);

    return result;
}
