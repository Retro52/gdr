#extension GL_EXT_shader_8bit_storage: require

#include "include/shaders/constants.h"

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
#if 0
    float ux, uy;
    float tx, ty, tz;
#endif
};

struct Meshlet
{
#if 0
    uint vertices[kMaxVerticesPerMeshlet];
    uint8_t indices[kMaxIndicesPerMeshlet];
#endif
    uint data_offset;   // offset (in bytes) into a shader vertices/indices array
    float cone[4];      // xyz - direction; w - alpha encoded in -127 to +127 range
    float bsphere[4];   // xyz - center, w - radius
    uint8_t vertices_count;
    uint8_t triangles_count;
    uint8_t padding[2];
};

struct MeshletTask
{
    uint meshlet_ids[kMaxVerticesPerMeshlet];
};
