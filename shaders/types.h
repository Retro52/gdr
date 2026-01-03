#extension GL_EXT_shader_8bit_storage: require

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float ux, uy;
    float tx, ty, tz;
};

const uint kMaxVerticesPerMeshlet  = 64;
const uint kMaxTrianglesPerMeshlet = 126;

struct Meshlet
{
    uint vertices[kMaxVerticesPerMeshlet];
    uint8_t indices[kMaxTrianglesPerMeshlet * 3];
    uint8_t vertices_count;
    uint8_t triangles_count;
    // float cull_cone[4];  // xyz - axis, w - angle
};
