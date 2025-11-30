#pragma once

#include <render/static_model.hpp>

namespace render
{
    template<typename T>
    VkFormat get_attribute_format();

    template<typename T>
    VkVertexInputBindingDescription get_vertex_binding();

    template<typename T>
    const VkVertexInputAttributeDescription* get_vertex_description(u32* size);

    template<>
    inline VkFormat get_attribute_format<f32>()
    {
        return VK_FORMAT_R32_SFLOAT;
    }

    template<>
    inline VkFormat get_attribute_format<glm::vec2>()
    {
        return VK_FORMAT_R32G32_SFLOAT;
    }

    template<>
    inline VkFormat get_attribute_format<glm::vec3>()
    {
        return VK_FORMAT_R32G32B32_SFLOAT;
    }

    template<>
    inline VkFormat get_attribute_format<glm::vec4>()
    {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    // static_model::static_model_vertex
    template<>
    inline VkVertexInputBindingDescription get_vertex_binding<static_model::static_model_vertex>()
    {
        return {.binding   = 0,
                .stride    = sizeof(static_model::static_model_vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    template<>
    inline const VkVertexInputAttributeDescription* get_vertex_description<static_model::static_model_vertex>(u32* size)
    {
        using self_t = static_model::static_model_vertex;

        static VkVertexInputAttributeDescription kVertexDesc[] = {
            {
             .location = 0,
             .binding  = 0,
             .format   = get_attribute_format<decltype(self_t::position)>(),
             .offset   = offsetof(self_t,       position),
             },
            {
             .location = 1,
             .binding  = 0,
             .format   = get_attribute_format<decltype(self_t::normal)>(),
             .offset   = offsetof(self_t,                     normal),
             },
            {
             .location = 2,
             .binding  = 0,
             .format   = get_attribute_format<decltype(self_t::uv)>(),
             .offset   = offsetof(self_t,                                               uv),
             },
            {
             .location = 3,
             .binding  = 0,
             .format   = get_attribute_format<decltype(self_t::tangent)>(),
             .offset   = offsetof(self_t,tangent),
             },
        };

        *size = DYE_ARR_SIZE(kVertexDesc);
        return kVertexDesc;
    }
}
