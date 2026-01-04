#extension GL_EXT_shader_8bit_storage: require

#include "include/shaders/constants.h"

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float ux, uy;
    float tx, ty, tz;
};

struct Meshlet
{
    uint vertices[kMaxVerticesPerMeshlet];
    uint8_t indices[kMaxIndicesPerMeshlet];
    uint8_t vertices_count;
    uint8_t triangles_count;
    float cone[4];   // xyz - direction; w - alpha encoded in -127 to +127 range
    float bsphere[4]; // xyz - center, w - radius
};

struct MeshletTask
{
    uint meshlet_ids[kMaxVerticesPerMeshlet];
};
