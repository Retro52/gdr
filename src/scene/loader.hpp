#pragma once

#include <fs/fs.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>
#include <shaders/constants.h>
#include <shaders/types.h>

class scene;

namespace mesh
{
    struct raw_mesh;
}

namespace loader
{
    constexpr static u32 kLODCount = shader_constants::kLODCount;

    using vertex    = shader_types::Vertex;
    using lod       = shader_types::MeshLod;
    using meshlet   = shader_types::Meshlet;
    using primitive = shader_types::MeshData;
    using material  = shader_types::MeshMaterial;
    using instance  = shader_types::MeshInstance;

    struct stats
    {
        u64 meshes     = 0;
        u64 meshlets   = 0;
        u64 triangles  = 0;
        u64 primitives = 0;
    };

    struct prim_layout
    {
        u64 vertex_offset;

        struct lod_layout
        {
            u64 index_offset;
            u64 meshlet_offset;
            u64 meshlet_data_offset;
        };

        u32 prim_index;  // index into ctx.primitives
        std::array<lod_layout, shader_constants::kLODCount> lod_array;
    };

    struct loader_context
    {
        cpp::heap_array<loader::vertex> vertices;
        cpp::heap_array<u32> indices;

        cpp::heap_array<loader::meshlet> meshlets;
        cpp::heap_array<u8> meshlets_data;

        cpp::heap_array<loader::material> materials;
        cpp::heap_array<loader::primitive> primitives;
    };

    result<render::vk_image> load_texture(const fs::path& path, const render::vk_renderer& renderer,
                                          const render::vk_buffer_transfer& scratch);

    stats load_scene(const fs::path& path, scene& scene, const render::vk_renderer& renderer,
                     render::vk_scene_geometry_pool& geometry_pool, cpp::heap_array<render::vk_image>& textures);

    void encode_raw_mesh(loader_context& ctx, const render::vk_scene_geometry_pool& geometry_pool,
                         const mesh::raw_mesh& primitive, const prim_layout& layout);
}
