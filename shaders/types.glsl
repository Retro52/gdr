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
    uint data_offset;   // offset (in bytes) into a shader vertices/indices array
    float cone_axis[3];
    float cone_cutoff;
    float sphere_center[3];
    float sphere_radius;
    uint8_t vertices_count;
    uint8_t triangles_count;
};

struct MeshletTask
{
    uint draw_id;
    uint meshlet_ids[kMaxVerticesPerMeshlet];
};

struct DrawCommand
{
    vec4 pos_and_scale;   // xyz - position, w - uniform scale
    vec4 rotation_quat;   // quaternion representing object position
    vec4 bounding_sphere; // bounding sphere of the whole mesh
    uint meshlets_count;
};

struct DrawMeshIndirect
{
    uint group_size[3];
};

struct DrawIndexedIndirect
{
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};
