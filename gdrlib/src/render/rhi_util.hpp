#pragma once

#include <pod_types.hpp>

#include <utility>

namespace render::rhi
{
    template<typename H, typename T>
    [[nodiscard]] H create_handle(T&& handle)
    {
        using raw_t = std::remove_cvref_t<T>;
        return H {.id = reinterpret_cast<u64>(new raw_t(std::forward<T>(handle)))};
    }

    template<typename H, typename T>
    [[nodiscard]] H reference_handle(const T* storage)
    {
        return H {.id = reinterpret_cast<u64>(storage)};
    }

    template<typename H>
    void clear_handle(const nullptr_t storage, H&& data)
    {
        memset(&data, 0, sizeof(data));
    }

    template<typename T, typename H>
    void clear_handle(T* storage, H&& data)
    {
        delete storage;
        memset(&data, 0, sizeof(data));
    }

    template<typename T, typename H>
    [[nodiscard]] T* cast_from_handle(H&& handle)
    {
        return reinterpret_cast<T*>(handle.id);
    }
}
