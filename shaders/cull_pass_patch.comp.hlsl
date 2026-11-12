#include "include/shaders/types.hlsl.h"

StructuredBuffer<uint>    draw_commands_count:   register(t0);
RWStructuredBuffer<uint3> draw_commands_threads: register(u1);

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 global_id: SV_DispatchThreadID)
{
    uint idx = global_id.x;
    if (idx >= kMatClassCount)
    {
        return;
    }

    uint mat_class_count = draw_commands_count[idx];
    draw_commands_threads[idx] = uint3(min(mat_class_count, kMaxTaskDispatchDimension), (mat_class_count + kMaxTaskDispatchDimension - 1) / kMaxTaskDispatchDimension, 1);
}
