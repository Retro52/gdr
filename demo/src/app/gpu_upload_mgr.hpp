#pragma once

#include <bytes.hpp>
#include <cpp/containers/heap_array.hpp>
#include <render/rhi.hpp>
#include <scene/loader.hpp>

#include <span>

namespace app
{
    struct mapped_buffer
    {
        void* mapped;
        rhi::buffer buffer;

        static mapped_buffer create(const rhi::impl& rhi, const rhi::context ctx, const u64 size,
                                    const rhi::buffer_usage_bits usage_flags)
        {
            mapped_buffer result {};
            const rhi::create_buffer_info info {
                .mapped      = &result.mapped,
                .size        = size,
                .usage_flags = usage_flags,
            };
            result.buffer = *rhi.create_buffer(ctx, info);

            return result;
        }

        static mapped_buffer create(const rhi::impl& rhi, const rhi::context ctx, rhi::create_buffer_info info)
        {
            mapped_buffer result {};
            info.mapped   = &result.mapped;
            result.buffer = *rhi.create_buffer(ctx, info);

            return result;
        }
    };

    struct gpu_upload_mgr
    {
    public:
        gpu_upload_mgr(rhi::impl& rhi, rhi::context ctx, u64 staging_buffer_size);

        template<typename T>
        void submit(const rhi::buffer dst, std::span<const T> data)
        {
            return this->submit<T>(dst, 0, data.data(), data.size());
        }

        template<typename T>
        void submit(const rhi::buffer dst, const T* data, const u64 count)
        {
            return this->submit<T>(dst, 0, data, count);
        }

        template<typename T>
        void submit(const rhi::buffer dst, const u64 dst_offset, std::span<const T> data)
        {
            return this->submit<T>(dst, dst_offset, data.data(), data.size());
        }

        template<typename T>
        void submit(const rhi::buffer dst, const u64 dst_offset, const T* data, const u64 count)
        {
            return this->submit_internal(dst, dst_offset, data, count * sizeof(T));
        }

        [[nodiscard]] rhi::image create_texture(const loader::texture_desc& texture) const;

    private:
        void submit_internal(rhi::buffer dst, u64 dst_offset, const void* data, u64 bytes) const;

    private:
        rhi::impl& m_rhi;
        rhi::context m_ctx;
        mapped_buffer m_staging_buffer;

        u64 m_max_capacity   = 0;
        u64 m_current_offset = 0;

        rhi::queue m_upload_queue         = rhi::null_queue;
        rhi::command_buffer m_staging_cmd = rhi::null_command_buffer;
    };
}
