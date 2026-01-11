#ifdef __cplusplus
#pragma once

namespace shader_constants
{
    using uint = unsigned int;
#endif

    const uint kMaxVerticesPerMeshlet  = 64;
    const uint kMaxTrianglesPerMeshlet = 94;
    const uint kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

    const uint kTaskWorkGroups = 32;
    const uint kMeshWorkGroups = 32;

#ifdef __cplusplus
}
#endif

#define USE_CONE_CULLING 1

#define VISUALIZE_MESHLETS          1
#define VISUALIZE_MESHLET_TRIANGLES 0
