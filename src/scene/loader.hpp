#pragma once

#include <fs/fs.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <shaders/constants.h>
#include <shaders/types.h>

class scene;

namespace loader
{
    constexpr static u32 kLODCount = shader_constants::kLODCount;

    constexpr static u32 kMaxIndicesPerMeshlet   = shader_constants::kMaxIndicesPerMeshlet;
    constexpr static u32 kMaxVerticesPerMeshlet  = shader_constants::kMaxVerticesPerMeshlet;
    constexpr static u32 kMaxTrianglesPerMeshlet = shader_constants::kMaxTrianglesPerMeshlet;

    using vertex    = shader_types::Vertex;
    using lod       = shader_types::MeshLod;
    using meshlet   = shader_types::Meshlet;
    using primitive = shader_types::MeshData;
    using instance  = shader_types::MeshInstance;

    struct mesh_desc
    {
        u32 offset;
        u32 prim_count;
    };

    struct mesh_data
    {
        cpp::heap_array<vertex> vertices;
        cpp::heap_array<u32> indices;
    };

    struct stats
    {
        u64 meshes     = 0;
        u64 meshlets   = 0;
        u64 triangles  = 0;
        u64 primitives = 0;
    };

    cpp::heap_array<mesh_data> load_mesh(const fs::path& path);

    stats load_scene(const fs::path& path, scene& scene, render::vk_scene_geometry_pool& geometry_pool);

    primitive upload_primitive(const mesh_data& data, render::vk_scene_geometry_pool& geometry_pool);
}
