#pragma once

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <cpp/hash/hashed_string.hpp>
#include <fs/fs.hpp>
#include <render/sm_serializer.hpp>
#include <render/static_model.hpp>
#include <Tracy/Tracy.hpp>

namespace
{
    constexpr u64 kSMMagic = "wazzup"_hs;

    struct mesh_header
    {
        u64 indices_count {0};
        u64 vertices_count {0};
        u64 vertices_stride {0};
    };

    struct model_header
    {
        u64 magic {kSMMagic};
        u64 meshes_count {0};
    };
}

result<bytes> render::serialize_mesh_cache(const u32* indices, u64 indices_count, const void* vertices,
                                           u64 vertices_count, u64 vertices_stride)
{
    ZoneScoped;
    if (!indices || !vertices || indices_count == 0 || vertices_count == 0)
    {
        return "corrupted data";
    }

    const u64 indices_size  = indices_count * sizeof(u32);
    const u64 vertices_size = vertices_count * vertices_stride;

    bytes result {sizeof(mesh_header) + indices_size + vertices_size};

    u64 data_pointer = 0;
    auto write       = [&](const void* data, u64 bytes)
    {
        cpp::cx_memcpy(result.get<u8>() + data_pointer, data, bytes);
        data_pointer += bytes;
    };

    const mesh_header header {
        .indices_count   = indices_count,
        .vertices_count  = vertices_count,
        .vertices_stride = vertices_stride,
    };

    write(&header, sizeof(header));
    write(indices, indices_size);
    write(vertices, vertices_size);

    return result;
}

void render::load_mesh_cache_data(const void* data, u32* indices, void* vertices, u64 indices_count, u64 vertices_count,
                                  u64 vertices_stride)
{
    ZoneScoped;

#if !defined(NDEBUG)
    u64 val_indices_count   = 0;
    u64 val_vertices_count  = 0;
    u64 val_vertices_stride = 0;
    load_mesh_cache_stats(data, val_indices_count, val_vertices_count, val_vertices_stride);
    assert2(val_indices_count == indices_count);
    assert2(val_vertices_count == vertices_count);
    assert2(val_vertices_stride == vertices_stride);
#endif

    u64 data_pointer = sizeof(mesh_header);
    auto read        = [&](void* dst, u64 bytes)
    {
        const auto* src = static_cast<const u8*>(data) + data_pointer;
        cpp::cx_memcpy(dst, src, bytes);
        data_pointer += bytes;
    };

    read(indices, indices_count * sizeof(u32));
    read(vertices, vertices_count * vertices_stride);
}

void render::load_mesh_cache_stats(const void* data, u64& indices_count, u64& vertices_count, u64& vertices_stride)
{
    ZoneScoped;

    mesh_header header;
    cpp::cx_memcpy(&header, data, sizeof(header));

    indices_count   = header.indices_count;
    vertices_count  = header.vertices_count;
    vertices_stride = header.vertices_stride;
}

result<bytes> render::serialize_model_cache(const bytes* meshes, u32 mesh_count)
{
    ZoneScoped;
    if (!meshes || mesh_count == 0)
    {
        return "corrupted data";
    }

    u64 data_size = 0;
    for (u32 i = 0; i < mesh_count; ++i)
    {
        data_size += meshes[i].size();
    }

    bytes result {sizeof(model_header) + data_size};

    u64 data_pointer = 0;
    auto write       = [&](const void* data, u64 bytes)
    {
        cpp::cx_memcpy(result.get<u8>() + data_pointer, data, bytes);
        data_pointer += bytes;
    };

    const model_header header {
        .magic        = kSMMagic,
        .meshes_count = mesh_count,
    };

    write(&header, sizeof(header));
    for (u32 i = 0; i < mesh_count; ++i)
    {
        write(meshes[i].data(), meshes[i].size());
    }

    return result;
}

result<bytes> render::load_model_cache(const fs::path& path, u32& mesh_count)
{
    ZoneScoped;
    auto file = fs::read_file(path);
    if (!file)
    {
        return file.message;
    }

    const u64 file_size      = file->size();
    const u64 mesh_data_size = file_size - sizeof(model_header);

    const auto* data = file->get<u8>();

    u64 data_pointer = 0;
    auto read        = [&](void* dst, u64 bytes)
    {
        if (data_pointer + bytes > file_size)
        {
            assert2(false && "read past data bounds");
            return;
        }

        cpp::cx_memcpy(dst, data + data_pointer, bytes);
        data_pointer += bytes;
    };

    model_header header;
    read(&header, sizeof(header));

    if (header.magic != kSMMagic || header.meshes_count == 0)
    {
        return header.magic != kSMMagic ? "version missmatch" : "no meshes were saved";
    }

    bytes result(mesh_data_size);

    mesh_count = header.meshes_count;
    read(result.get<u8>(), mesh_data_size);

    return result;
}
