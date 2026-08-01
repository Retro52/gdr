// ReSharper disable once CppMissingIncludeGuard
#ifdef __cplusplus
#pragma once

namespace shader_bindings::shadow_cull
{
    using uint = unsigned int;
#endif

    const uint kPrimitiveBinding   = 0;
    const uint kInstanceBinding    = 1;
    const uint kMaterialBinding    = 2;
    const uint kDrawCountBinding   = 3;
    const uint kFrameCullBinding   = 4;
    const uint kCascadeCullBinding = 5;
    const uint kOutDrawBinding     = 6;
#ifdef __cplusplus
}
#endif
