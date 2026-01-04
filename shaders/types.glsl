#extension GL_EXT_shader_8bit_storage : require

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float ux, uy;
    float tx, ty, tz;
};

const uint kMaxVerticesPerMeshlet  = 64;
const uint kMaxTrianglesPerMeshlet = 94;
const uint kMaxIndicesPerMeshlet = kMaxTrianglesPerMeshlet * 3;

struct Meshlet
{
    uint vertices[kMaxVerticesPerMeshlet];
    uint8_t indices[kMaxIndicesPerMeshlet];
    uint8_t vertices_count;
    uint8_t triangles_count;
    float cull_cone[4]; // cull cone; xyz is a direction, a - cone angle
};
