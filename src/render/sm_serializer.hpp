#pragma once

#include <types.hpp>

#include <bytes.hpp>
#include <fs/path.hpp>
#include <result.hpp>

namespace render
{
    result<bytes> serialize_mesh_cache(const u32* indices, u64 indices_count, const void* vertices, u64 vertices_count,
                                       u64 vertices_stride);

    void load_mesh_cache_stats(const void* data, u64& indices_count, u64& vertices_count, u64& vertices_stride);

    void load_mesh_cache_data(const void* data, u32* indices, void* vertices, u64 indices_count, u64 vertices_count, u64 vertices_stride);

    result<bytes> serialize_model_cache(const bytes* meshes, u32 mesh_count);

    result<bytes> load_model_cache(const fs::path& path, u32& mesh_count);
}
