#pragma once

#include <bytes.hpp>
#include <cpp/containers/heap_array.hpp>
#include <fs/fs.hpp>
#include <shaders/constants.h>
#include <shaders/types.h>

#include <array>

class scene;
struct cgltf_primitive;

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

    struct texture_desc
    {
        uvec3 dimensions         = uvec3(0);
        u32 mips_count           = 0;
        u32 arrays_count         = 0;
        u32 block_size           = 0;
        u32 bits_per_block       = 0;
        rhi::image_format format = rhi::image_format::none;
        bytes pdata;
    };

    struct scene_counters
    {
        u64 meshlets  = 0;
        u64 triangles = 0;
        u64 instances = 0;
        std::array<u32, shader_constants::kMatClassCount> mat_offset_table {};
    };

    struct prim_layout
    {
        u64 vertex_offset;

        struct lod_layout
        {
            u64 meshlet_offset;
            u64 meshlet_data_offset;
        };

        u32 prim_index;  // index into ctx.primitives
        std::array<lod_layout, shader_constants::kLODCount> lod_array;
    };

    struct mesh_info
    {
        u32 offset;
        u32 prim_count;
    };

    struct prim_info
    {
        u64 id;
        const cgltf_primitive* ptr;
    };

    struct meshes_data
    {
        cpp::heap_array<mesh_info> meshes;
        cpp::heap_array<mesh::raw_mesh> primitives;
    };

    struct scene_data
    {
        scene_counters counters;

        cpp::heap_array<loader::vertex> vertices;

        cpp::heap_array<loader::meshlet> meshlets;
        cpp::heap_array<u8> meshlets_data;

        cpp::heap_array<loader::instance> instances;
        cpp::heap_array<loader::material> materials;
        cpp::heap_array<loader::primitive> primitives;
        cpp::heap_array<loader::texture_desc> textures;
    };

    u32 get_max_lod_tris(const mesh::raw_mesh& mesh);

    u32 get_max_lod_meshlets(const loader::primitive& prim);

    result<meshes_data> load_meshes(const fs::path& path);

    result<texture_desc> load_texture(const fs::path& path);

    result<scene_data> load_scene(const fs::path& path, scene& scene);

    void encode_raw_mesh(scene_data& ctx, const mesh::raw_mesh& primitive, const prim_layout& layout);
}
