#pragma once

#include <volk.h>

#include <types.hpp>

#include <cpp/tagged_int.hpp>
#include <reflection/enum.hpp>
#include <render/rhi_handles.hpp>

#include <span>

namespace rhi
{
    // clang-format off
    REGISTER_FLAGS(feature_flag,
        validation          = 1 << 0,
        mesh_shading        = 1 << 1,
        dynamic_render      = 1 << 2,
        synchronization2    = 1 << 3,
        draw_indirect       = 1 << 4,
        types_int8          = 1 << 5,
        pipeline_stats      = 1 << 6,
        sampler_min_max     = 1 << 7,
        scalar_block_layout = 1 << 8,
        portability_subset  = 1 << 9,
        bindless_textures   = 1 << 10,
        types_16bit         = 1 << 11,
        COUNT
    );

    REGISTER_ENUM(queue_kind,
        gfx,
        present,
        compute,
        transfer,
        COUNT
    );

    REGISTER_ENUM(shader_stage,
        task,
        mesh,
        vertex,
        compute,
        fragment,
        COUNT
    );

    REGISTER_ENUM(cull_mode,
        all,
        none,
        back,
        front,
        COUNT
    );

    REGISTER_ENUM(topology,
        list_points,
        list_patches,

        list_lines,
        list_triangles,
        list_lines_adjacent,
        list_triangles_adjacent,

        strip_lines,
        strip_triangles,
        strip_lines_adjacent,
        strip_triangles_adjacent,

        COUNT
    );

    REGISTER_ENUM(blend_op,
        min,
        max,
        add,
        subtract,
        reverse_subtract,
        COUNT
    );

    REGISTER_ENUM(blend_factor,
        zero,
        one,

        src_color,
        src_alpha,
        one_minus_src_color,
        one_minus_src_alpha,

        dst_color,
        dst_alpha,
        one_minus_dst_color,
        one_minus_dst_alpha
    );

    REGISTER_ENUM(compare_op,
        none,
        never,
        always,
        less,
        greater,
        equal,
        not_equal,
        equal_or_less,
        equal_or_greater,
        COUNT
    );

    REGISTER_ENUM(resource_load_op,
        load,
        clear,
        discard,
        COUNT
    );

    REGISTER_ENUM(resource_store_op,
        store,
        discard,
        COUNT
    );

    REGISTER_FLAGS(barrier_stage,
        none                  = 0,

        indirect              = 1 << 0,
        vertex_input          = 1 << 1,

        vertex_shader         = 1 << 2,
        task_shader           = 1 << 3,
        mesh_shader           = 1 << 4,
        fragment_shader       = 1 << 5,
        compute_shader        = 1 << 6,

        early_depth_stencil   = 1 << 7,
        late_depth_stencil    = 1 << 8,
        color_attachment      = 1 << 9,

        copy                  = 1 << 10,

        all_graphics          = 1 << 11,
        all_commands          = 1 << 12,

        COUNT
    );

    REGISTER_FLAGS(barrier_access,
        none                  = 0,

        indirect_read         = 1 << 0,
        index_read            = 1 << 1,
        vertex_read           = 1 << 2,
        uniform_read          = 1 << 3,

        sampled_read          = 1 << 4,
        storage_read          = 1 << 5,
        storage_write         = 1 << 6,

        color_attachment_read  = 1 << 7,
        color_attachment_write = 1 << 8,

        depth_stencil_read    = 1 << 9,
        depth_stencil_write   = 1 << 10,

        copy_read             = 1 << 11,
        copy_write            = 1 << 12,

        COUNT
    );

    REGISTER_ENUM(sampler_filter,
        linear,
        nearest,
        COUNT
    );

    REGISTER_ENUM(sampler_mipmap_mode,
        linear,
        nearest,
        COUNT
    );

    REGISTER_ENUM(sampler_reduction,
        none, // mb default or average would be a better name tbh
        min,
        max,
        COUNT
    );

    REGISTER_ENUM(sampler_border_color,
        white,
        black,
        black_transparent,
        COUNT
    );

    REGISTER_ENUM(sampler_address_mode,
        repeat,
        clamp_to_edge,
        clamp_to_border,
        mirrored_repeat,
        mirrored_clamp_to_edge,
        COUNT
    );

    // I hate these custom enums that try to cover a gajillion different formats from the API they abstract,
    // so fuck it, VkFormat is my new universal enum
    // The only reason this enum exists is a sanity check to not use some crazy value that exists on some
    // dinosaur years old hardware, or the opposite - some workstation class GPU for specialized workloads
    // Oh, and naming! You look adorable, etc2_r8g8b8a1_srgb, GET OUTTA HERE VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK
    REGISTER_ENUM(image_format,
        none                = VK_FORMAT_UNDEFINED,

        // depth / stencil
        d16un               = VK_FORMAT_D16_UNORM,
        d32sf               = VK_FORMAT_D32_SFLOAT,
        d32sf_s8ui          = VK_FORMAT_D32_SFLOAT_S8_UINT,

        // 8 bit
        r8un                = VK_FORMAT_R8_UNORM,
        r8sn                = VK_FORMAT_R8_SNORM,
        r8ui                = VK_FORMAT_R8_UINT,
        r8si                = VK_FORMAT_R8_SINT,

        r8g8un              = VK_FORMAT_R8G8_UNORM,
        r8g8sn              = VK_FORMAT_R8G8_SNORM,
        r8g8ui              = VK_FORMAT_R8G8_UINT,
        r8g8si              = VK_FORMAT_R8G8_SINT,

        r8g8b8a8un          = VK_FORMAT_R8G8B8A8_UNORM,
        r8g8b8a8sn          = VK_FORMAT_R8G8B8A8_SNORM,
        r8g8b8a8ui          = VK_FORMAT_R8G8B8A8_UINT,
        r8g8b8a8si          = VK_FORMAT_R8G8B8A8_SINT,
        r8g8b8a8srgb        = VK_FORMAT_R8G8B8A8_SRGB,

        b8g8r8a8un          = VK_FORMAT_B8G8R8A8_UNORM,
        b8g8r8a8srgb        = VK_FORMAT_B8G8R8A8_SRGB,

        // 16 bit
        r16un               = VK_FORMAT_R16_UNORM,
        r16sn               = VK_FORMAT_R16_SNORM,
        r16ui               = VK_FORMAT_R16_UINT,
        r16si               = VK_FORMAT_R16_SINT,
        r16sf               = VK_FORMAT_R16_SFLOAT,

        r16g16un            = VK_FORMAT_R16G16_UNORM,
        r16g16sn            = VK_FORMAT_R16G16_SNORM,
        r16g16ui            = VK_FORMAT_R16G16_UINT,
        r16g16si            = VK_FORMAT_R16G16_SINT,
        r16g16sf            = VK_FORMAT_R16G16_SFLOAT,

        r16g16b16a16un      = VK_FORMAT_R16G16B16A16_UNORM,
        r16g16b16a16sn      = VK_FORMAT_R16G16B16A16_SNORM,
        r16g16b16a16ui      = VK_FORMAT_R16G16B16A16_UINT,
        r16g16b16a16si      = VK_FORMAT_R16G16B16A16_SINT,
        r16g16b16a16sf      = VK_FORMAT_R16G16B16A16_SFLOAT,

        // 32 bit
        r32ui               = VK_FORMAT_R32_UINT,
        r32si               = VK_FORMAT_R32_SINT,
        r32sf               = VK_FORMAT_R32_SFLOAT,

        r32g32ui            = VK_FORMAT_R32G32_UINT,
        r32g32si            = VK_FORMAT_R32G32_SINT,
        r32g32sf            = VK_FORMAT_R32G32_SFLOAT,

        r32g32b32a32ui      = VK_FORMAT_R32G32B32A32_UINT,
        r32g32b32a32si      = VK_FORMAT_R32G32B32A32_SINT,
        r32g32b32a32sf      = VK_FORMAT_R32G32B32A32_SFLOAT,

        // packed
        a2b10g10r10un       = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        a2b10g10r10ui       = VK_FORMAT_A2B10G10R10_UINT_PACK32,
        a2r10g10b10un       = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        b10g11r11uf         = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        e5b9g9r9uf          = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

        // BC compressed
        bc1_rgb_un          = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        bc1_rgb_srgb        = VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        bc1_rgba_un         = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        bc1_rgba_srgb       = VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        bc3_un              = VK_FORMAT_BC3_UNORM_BLOCK,
        bc3_srgb            = VK_FORMAT_BC3_SRGB_BLOCK,
        bc4_un              = VK_FORMAT_BC4_UNORM_BLOCK,
        bc4_sn              = VK_FORMAT_BC4_SNORM_BLOCK,
        bc5_un              = VK_FORMAT_BC5_UNORM_BLOCK,
        bc5_sn              = VK_FORMAT_BC5_SNORM_BLOCK,
        bc6h_uf             = VK_FORMAT_BC6H_UFLOAT_BLOCK,
        bc6h_sf             = VK_FORMAT_BC6H_SFLOAT_BLOCK,
        bc7_un              = VK_FORMAT_BC7_UNORM_BLOCK,
        bc7_srgb            = VK_FORMAT_BC7_SRGB_BLOCK,

        // ETC2 / EAC compressed
        etc2_r8g8b8_un      = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
        etc2_r8g8b8_srgb    = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
        etc2_r8g8b8a1_un    = VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        etc2_r8g8b8a1_srgb  = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
        etc2_r8g8b8a8_un    = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
        etc2_r8g8b8a8_srgb  = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        eac_r11_un          = VK_FORMAT_EAC_R11_UNORM_BLOCK,
        eac_r11_sn          = VK_FORMAT_EAC_R11_SNORM_BLOCK,
        eac_r11g11_un       = VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
        eac_r11g11_sn       = VK_FORMAT_EAC_R11G11_SNORM_BLOCK,

        // ASTC compressed
        astc_4x4_un         = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        astc_4x4_srgb       = VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        astc_5x5_un         = VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        astc_5x5_srgb       = VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        astc_6x6_un         = VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        astc_6x6_srgb       = VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        astc_8x8_un         = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        astc_8x8_srgb       = VK_FORMAT_ASTC_8x8_SRGB_BLOCK
    );

    REGISTER_FLAGS(image_usage,
        sampled          = 1 << 0,
        storage          = 1 << 1,
        attachment_color = 1 << 2,
        attachment_ds    = 1 << 3,
        transfer_src     = 1 << 4,
        transfer_dst     = 1 << 5,
        COUNT
    );

    REGISTER_ENUM(image_layout,
        discard,
        current,
        common,
        present,
        render_target_color,
        render_target_depth_stencil,
        COUNT
    );

    REGISTER_FLAGS(image_aspect,
        color   = 1 << 0,
        depth   = 1 << 1,
        stencil = 1 << 2,
        format  = 1 << 3, // for functions that accept image format as well we can just derive the aspect from the format, most of the time

        COUNT
    );

    REGISTER_ENUM(image_view_kind,
        flat_1d,
        flat_2d,
        flat_cube,

        array_1d,
        array_2d,
        array_cube,

        flat_3d,
        COUNT
    );

    REGISTER_FLAGS(buffer_usage,
        copy_src  = 1 << 0, // == TRANSFER_SRC
        copy_dst  = 1 << 1, // == TRANSFER_DST
        shader_rw = 1 << 2, // == STORAGE_BUFFER
        indirect  = 1 << 3, // == INDIRECT_BUFFER
        index     = 1 << 4, // == INDEX_BUFFER
        COUNT
    );
    // clang-format on

    constexpr u32 kMipsAll   = ~static_cast<u32>(0);
    constexpr u32 kLayersAll = ~static_cast<u32>(0);
    constexpr u64 kBufferAll = ~static_cast<u64>(0);

    constexpr u32 kQueueTypesCount = static_cast<u32>(queue_kind::COUNT);

    union color_clear_value
    {
        vec4 f4;
        uvec4 u4;
        ivec4 i4;
    };

    struct ds_clear_value
    {
        f32 depth;
        u8 stencil;
    };

    union clear_value
    {
        color_clear_value color;
        ds_clear_value ds;
    };

    struct barrier_scope
    {
        barrier_stage_bits stages  = barrier_stage::none;
        barrier_access_bits access = barrier_access::none;
    };

    struct image_subresource_range
    {
        image_aspect_bits aspects = image_aspect::format;

        uvec2 mips_range   = uvec2(0, kMipsAll);
        uvec2 layers_range = uvec2(0, kLayersAll);
    };

    struct global_barrier
    {
        barrier_scope before;
        barrier_scope after;
    };

    struct buffer_barrier
    {
        rhi::buffer buffer = null_buffer;

        barrier_scope before;
        barrier_scope after;

        u64vec2 data_range = u64vec2(0, kBufferAll);
    };

    struct image_barrier
    {
        rhi::image image = null_image;

        barrier_scope before;
        barrier_scope after;

        image_layout layout_before = image_layout::current;
        image_layout layout_after  = image_layout::current;

        image_subresource_range range;
    };

    struct barrier_batch
    {
        std::span<const image_barrier> images;
        std::span<const global_barrier> globals;
        std::span<const buffer_barrier> buffers;
    };

    struct create_sampler_info
    {
        sampler_filter filter             = sampler_filter::linear;
        sampler_reduction reduction       = sampler_reduction::none;
        sampler_mipmap_mode mipmap_mode   = sampler_mipmap_mode::linear;
        sampler_address_mode address_mode = sampler_address_mode::repeat;

        u32 anisotropy_factor             = 0;  // 0 = disable; >0 = enabled and uses the factor specified
        vec2 lod_range                    = vec2(0.0F, 16.0F);
        compare_op compare_op             = compare_op::none;
        sampler_border_color border_color = sampler_border_color::black_transparent;
    };

    struct create_image_info
    {
        image_format format = image_format::none;

        u32 mips_count               = 1;
        u32 layer_count              = 1;
        uvec3 dimensions             = uvec3(1, 1, 1);
        image_usage_bits usage_flags = image_usage::sampled;
    };

    struct create_image_view_info
    {
        image_subresource_range range;

        image_format format  = image_format::none;
        image_view_kind kind = image_view_kind::flat_2d;
    };

    struct create_buffer_info
    {
        void** mapped = nullptr;  // if not null => will create mapped buffer. smart, huh?)
        u64 size      = 0;

        buffer_usage_bits usage_flags = buffer_usage::shader_rw | buffer_usage::copy_dst;
    };

    struct blit_image_info
    {
        ivec3 src_offset = ivec3(0, 0, 0);
        ivec3 src_extent = ivec3(1, 1, 1);
        uvec2 src_layers = uvec2(0, kLayersAll);
        u32 src_mip      = 0;

        ivec3 dst_offset = ivec3(0, 0, 0);
        ivec3 dst_extent = ivec3(1, 1, 1);
        uvec2 dst_layers = uvec2(0, kLayersAll);
        u32 dst_mip      = 0;

        image_aspect_bits aspects = image_aspect::color;
        sampler_filter filter     = sampler_filter::linear;
    };

    struct copy_image_info
    {
        u64 source_offset = 0;

        u32 dst_mip               = 0;
        uvec2 dst_layers          = uvec2(0, kLayersAll);
        image_aspect_bits aspects = image_aspect::color;

        ivec3 dst_offset = uvec3(0, 0, 0);
        uvec3 dst_extent = uvec3(1, 1, 1);
    };

    struct attachment
    {
        constexpr attachment(const image img)
            : m_raw_handle_id(img.id, true)
        {
        }

        constexpr attachment(const image_view view)
            : m_raw_handle_id(view.id, false)
        {
        }

        [[nodiscard]] bool is_image_view() const noexcept { return !m_raw_handle_id.get_flag(); }

        [[nodiscard]] image get_image() const noexcept { return image {.id = m_raw_handle_id.value()}; }

        [[nodiscard]] image_view get_image_view() const noexcept { return image_view {.id = m_raw_handle_id.value()}; }

    private:
        friend struct binding;
        friend struct attachment_state_info;

        [[nodiscard]] constexpr bool valid() const noexcept { return m_raw_handle_id.value() != null_image.id; }

        cpp::tagged_u64 m_raw_handle_id;
    };

    struct binding
    {
        binding() = default;

        constexpr binding(const buffer buffer)
            : m_buffer_or_sampler(buffer.id, true)
        {
        }

        constexpr binding(const sampler sampler)
            : m_buffer_or_sampler(sampler.id, false)
        {
        }

        constexpr binding(const attachment attachment)
            : m_attachment(attachment)
        {
        }

        constexpr binding(const attachment attachment, const sampler sampler)
            : m_attachment(attachment)
            , m_buffer_or_sampler(sampler.id, false)
        {
        }

        [[nodiscard]] bool has_buffer() const noexcept
        {
            return m_buffer_or_sampler.get_flag() && m_buffer_or_sampler.value() != null_buffer.id;
        }

        [[nodiscard]] buffer get_buffer() const noexcept { return buffer {.id = m_buffer_or_sampler.value()}; }

        [[nodiscard]] bool has_sampler() const noexcept
        {
            return !m_buffer_or_sampler.get_flag() && m_buffer_or_sampler.value() != null_sampler.id;
        }

        [[nodiscard]] sampler get_sampler() const noexcept { return sampler {.id = m_buffer_or_sampler.value()}; }

        [[nodiscard]] bool has_attachment() const noexcept { return m_attachment.valid(); }

        [[nodiscard]] attachment get_attachment() const noexcept { return m_attachment; }

    private:
        attachment m_attachment             = null_image;
        cpp::tagged_u64 m_buffer_or_sampler = null_buffer.id;
    };

    struct attachment_state_info
    {
        attachment attachment;
        resource_load_op load_op   = resource_load_op::load;
        resource_store_op store_op = resource_store_op::store;
        clear_value clear_value    = {.color = {.f4 = vec4(0.0F)}};

        constexpr operator bool() const noexcept { return attachment.valid(); }
    };

    constexpr auto null_attachment_state_info = attachment_state_info {.attachment = attachment(null_image)};

    struct bindless_set_write_info
    {
        attachment dst;
        u32 index = 0;
    };

    struct create_swapchain_info
    {
        ivec2 size;
        u32 frames_in_flight = 2;
        image_format format  = image_format::r8g8b8a8un;
        bool vsync           = false;
    };

    struct fence_submit_info
    {
        fence fence = null_fence;
        u64 value   = 0;
    };

    struct submit_info
    {
        std::span<const fence_submit_info> waits;
        std::span<const fence_submit_info> signals;
        std::span<const command_buffer> command_buffers;
    };

    struct rendering_features_table
    {
        [[nodiscard]] constexpr bool required(const feature_flag flag) const noexcept
        {
            return flag & required_features;
        }

        [[nodiscard]] constexpr bool requested(const feature_flag flag) const noexcept
        {
            return flag & requested_features || required(flag);
        }

        [[nodiscard]] constexpr bool supported(const feature_flag flag) const noexcept
        {
            return flag & supported_features;
        }

        [[nodiscard]] constexpr bool wanted(const feature_flag flag) const noexcept
        {
            return (flag & supported_features) && requested(flag);
        }

        constexpr rendering_features_table& require(const feature_flag flag) noexcept
        {
            required_features = required_features | flag;
            return *this;
        }

        constexpr rendering_features_table& request(const feature_flag flag) noexcept
        {
            requested_features = requested_features | flag;
            return *this;
        }

        constexpr void set_supported(const feature_flag flag, const bool supported = true) noexcept
        {
            supported_features = supported ? supported_features | flag : supported_features & ~flag;
        }

        [[nodiscard]] constexpr bool all_required_supported() const noexcept
        {
            bool result = true;
            for (u32 i = 0; i < cpp::cx_get_enum_bit_count(feature_flag::COUNT); ++i)
            {
                const auto feature = static_cast<feature_flag>(1 << i);
                result &= this->required(feature) ? this->supported(feature) : result;
            }

            return result;
        }

        feature_flag_bits required_features;
        feature_flag_bits requested_features;
        feature_flag_bits supported_features;
    };

    struct instance_desc
    {
        const char* app_name = "";
        u32 app_version      = 0;
        u32 device_id_hint   = static_cast<u32>(-1);
        rendering_features_table device_features;
    };
}
