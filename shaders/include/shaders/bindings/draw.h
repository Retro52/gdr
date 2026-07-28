// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once

namespace shader_bindings::draw
{
    using uint = unsigned int;
#endif

    const uint kVertexBinding      = 0;
    const uint kMaterialBinding    = 1;
    const uint kTextureBinding     = 2;
    const uint kMeshletBinding     = 3;
    const uint kMeshletDataBinding = 4;
    const uint kPrimitiveBinding   = 5;
    const uint kInstanceBinding    = 6;
    const uint kDrawBinding        = 7;
    const uint kCullBinding        = 8;
    const uint kVisibilityBinding  = 9;
    const uint kHiZBinding         = 10;
#ifdef __cplusplus
}
#endif
