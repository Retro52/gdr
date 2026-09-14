#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_error.hpp>

void render::destroy_descriptor_set(const VkDevice device, vk_descriptor_set& descriptor_pool)
{
    VK_DESTROY(descriptor_pool.descriptor_set_layout, vkDestroyDescriptorSetLayout, device);
    VK_DESTROY(descriptor_pool.descriptor_pool, vkDestroyDescriptorPool, device);
    descriptor_pool.descriptor_pool = VK_NULL_HANDLE;
}

result<render::vk_descriptor_set> render::create_bindless_textures_set(const VkDevice device, const u32 max_textures)
{
    VkDescriptorPoolSize pool_sizes_bindless[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_textures}
    };

    const VkDescriptorPoolCreateInfo create_info {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .pNext = nullptr,
                                                  .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                                  .maxSets =
                                                      max_textures * static_cast<u32>(COUNT_OF(pool_sizes_bindless)),
                                                  .poolSizeCount = COUNT_OF(pool_sizes_bindless),
                                                  .pPoolSizes    = pool_sizes_bindless};

    VkDescriptorPool descriptor_pool;
    VK_RETURN_ON_FAIL(vkCreateDescriptorPool(device, &create_info, nullptr, &descriptor_pool));

    VkDescriptorSetLayoutBinding set_layout_bindings[] = {
        {0,
         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_textures,
         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
         nullptr}
    };

    VkDescriptorBindingFlags binding_flags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                                                | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount  = COUNT_OF(binding_flags),
        .pBindingFlags = binding_flags,
    };

    const VkDescriptorSetLayoutCreateInfo layout_create_info {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = &binding_flags_create_info,
        .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = COUNT_OF(set_layout_bindings),
        .pBindings    = set_layout_bindings,
    };

    VkDescriptorSetLayout descriptor_set_layout;
    VK_RETURN_ON_FAIL(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));

    const VkDescriptorSetVariableDescriptorCountAllocateInfo set_count_alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts  = &max_textures,
    };

    const VkDescriptorSetAllocateInfo allocate_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = &set_count_alloc_info,
        .descriptorPool     = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &descriptor_set_layout,
    };

    VkDescriptorSet descriptor_set;
    VK_RETURN_ON_FAIL(vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set));

    return render::vk_descriptor_set(descriptor_set, descriptor_pool, descriptor_set_layout);
}
