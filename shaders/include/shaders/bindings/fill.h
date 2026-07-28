// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once

namespace shader_bindings::fill
{
    using uint = unsigned int;
#endif

    const uint kMeshletBinding     = 0;
    const uint kMeshletDataBinding = 1;
    const uint kPrimitiveBinding   = 2;
    const uint kInstanceBinding    = 3;
    const uint kDrawBinding        = 4;
    const uint kCullBinding        = 5;
    const uint kOutIndicesBinding  = 6;
    const uint kOutCommandsBinding = 7;
    const uint kOutCountBinding    = 8;
    const uint kVisibilityBinding  = 9;
    const uint kHiZBinding         = 10;
#ifdef __cplusplus
}
#endif
