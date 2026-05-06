#include <app/gpu_stats.hpp>
#include <render/platform/vk/vk_error.hpp>

app::frame_statistics_data app::query_frame_statistics_data(VkDevice device, render::vk_query query)
{
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

app::pipeline_statistics_data app::query_pipeline_statistics_data(VkDevice device, render::vk_query query)
{
    if (!query.handle)
    {
        return {};
    }

    app::pipeline_statistics_data result;
    VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(
        device, query.handle, 0, 1, sizeof(result), &result, sizeof(u64), VK_QUERY_RESULT_64_BIT));

    return result;
}
