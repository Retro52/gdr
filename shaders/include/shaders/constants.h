#ifdef __cplusplus
#pragma once

namespace shader_constants
{
    using uint = unsigned int;
#endif

#define VISUALIZE_MESHLETS          0
#define VISUALIZE_MESHLET_TRIANGLES 0

    const uint kLodFlagBit            = 1;
    const uint kFrustumCullBit        = 2;
    const uint kOcclusionCullBit      = 3;
    const uint kMeshletConeCullBit    = 4;
    const uint kMeshletFrustumCullBit = 5;

    const uint kMaxVerticesPerMeshlet  = 64;
    const uint kMaxTrianglesPerMeshlet = 94;
    const uint kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

    const uint kLODCount = 8;

    const uint kTaskWorkGroups = 32;
    const uint kMeshWorkGroups = 32;
#ifdef __cplusplus
}
#endif

#define GET_BIT(flags, bit)   ((flags >> bit) & 1)
#define CHECK_BIT(flags, bit) GET_BIT(flags, bit) == 1
