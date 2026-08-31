#include <app/gpu_stats.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <tracy/Tracy.hpp>

app::frame_statistics_data app::query_frame_statistics_data(VkDevice device, render::vk_query query)
{
    ZoneScoped;
    if (!query.handle)
    {
        return {};
    }

    app::frame_statistics_data result;
    VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(device,
                                            query.handle,
                                            0,
                                            sizeof(result) / sizeof(u64),
                                            sizeof(result),
                                            &result,
                                            sizeof(u64),
                                            VK_QUERY_RESULT_64_BIT));

    return result;
}

app::pipeline_statistics_data app::query_pipeline_statistics_data(VkDevice device, render::vk_query query,
                                                                  u32 query_idx)
{
    ZoneScoped;
    if (!query.handle)
    {
        return {};
    }

    app::pipeline_statistics_data result;
    VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(
        device, query.handle, query_idx, 1, sizeof(result), &result, sizeof(u64), VK_QUERY_RESULT_64_BIT));

    return result;
}

app::pipeline_statistics_data app::operator+(const pipeline_statistics_data& a, const pipeline_statistics_data& b)
{
    return {
        a.input_assembly_vertices + b.input_assembly_vertices,
        a.input_assembly_primitives + b.input_assembly_primitives,
        a.vertex_shader_invocations + b.vertex_shader_invocations,
        a.triangles_count + b.triangles_count,
        a.fragment_shader_invocations + b.fragment_shader_invocations,
    };
}
