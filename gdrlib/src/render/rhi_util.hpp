#pragma once

#include <pod_types.hpp>

#include <utility>

namespace render::rhi
{
    template<typename H, typename T>
    [[nodiscard]] H create_handle(T& handle)
    {
        return H {.id = reinterpret_cast<u64>(new T(std::move(handle)))};
    }

    template<typename H, typename T>
    [[nodiscard]] H reference_handle(const T* storage)
    {
        return H {.id = reinterpret_cast<u64>(storage)};
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
