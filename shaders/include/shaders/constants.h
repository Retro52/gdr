// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once

namespace shader_constants
{
    using uint = unsigned int;
#endif

#define SHADERS_DEBUG 1

    const uint kLodFlagBit              = 0;
    const uint kFrustumCullBit          = 1;
    const uint kOcclusionCullBit        = 2;
    const uint kMeshletConeCullBit      = 3;
    const uint kMeshletFrustumCullBit   = 4;
    const uint kMeshletOcclusionCullBit = 5;
    const uint kSmallMeshletsCullBit    = 6;

    const uint kMatGlossBit = 0;

    const uint kMaxVerticesPerMeshlet  = 64;
    const uint kMaxTrianglesPerMeshlet = 94;
    const uint kMaxIndicesPerMeshlet   = kMaxTrianglesPerMeshlet * 3;

    const uint kLODCount = 8;

    const uint kTaskWorkGroups = 64;
    const uint kMeshWorkGroups = 64;

    const uint kEnvironmentMaxMips = 5;
#ifdef __cplusplus
}
#endif

#define GET_BIT(flags, bit)   ((flags >> bit) & 1)
#define CHECK_BIT(flags, bit) GET_BIT(flags, bit) == 1
