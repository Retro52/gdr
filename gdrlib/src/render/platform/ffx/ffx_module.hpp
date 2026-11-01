#pragma once

#if defined(_WIN32)

#include <pod_types.hpp>
#include <render/platform/ffx/ffx_types.hpp>

namespace platform
{
    struct ffx_module
    {
        decltype(&ffxQuery) ffxQuery                   = nullptr;
        decltype(&ffxConfigure) ffxDispatch            = nullptr;
        decltype(&ffxConfigure) ffxConfigure           = nullptr;
        decltype(&ffxCreateContext) ffxCreateContext   = nullptr;
        decltype(&ffxDestroyContext) ffxDestroyContext = nullptr;

        ffx_module() = default;

        ~ffx_module();

        void lazy_load(const char* path);

        operator bool() const noexcept { return m_handle != 0; }

        template<class... Desc>
        auto CreateContext(ffx::Context& context, ffxAllocationCallbacks* memCb, Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxCreateContext(&context, header, memCb));
        }

        auto DestroyContext(ffx::Context& context, const ffxAllocationCallbacks* memCb = nullptr) const
        {
            return ffx::detail::ConvertReturnCode(this->ffxDestroyContext(&context, memCb));
        }

        template<class... Desc>
        auto Configure(ffx::Context& context, Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxConfigure(&context, header));
        }

        template<class... Desc>
        auto Configure(Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxConfigure(nullptr, header));
        }

        template<class... Desc>
        auto Query(ffx::Context& context, Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxQuery(&context, header));
        }

        template<class... Desc>
        auto Query(Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxQuery(nullptr, header));
        }

        template<class... Desc>
        auto Dispatch(ffx::Context& context, Desc&... desc) const
        {
            auto header = ffx::LinkHeaders(desc.header...);
            return ffx::detail::ConvertReturnCode(this->ffxDispatch(&context, header));
        }

    private:
        u64 m_handle = 0;
    };
}
#endif
