#pragma once

#include <types.hpp>

#include <cpp/containers/heap_array.hpp>
#include <shaders/constants.h>

struct cgltf_primitive;

namespace mesh
{
    struct raw_vertex
    {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec4 tangent;
    };

    struct raw_meshlet
    {
        vec3 cone_axis;
        float cone_cutoff;
        vec3 sphere_center;
        float sphere_radius;
        u64 data_prefix_sum;
        u8 vertices_count;
        u8 triangles_count;
    };

    struct raw_lod
    {
        cpp::heap_array<u32> raw_indices;
        cpp::heap_array<u8> raw_meshlets_payload;
        cpp::heap_array<raw_meshlet> raw_meshlets;
        f32 error = 0.0F;
    };

    struct raw_mesh
    {
        cpp::heap_array<raw_vertex> raw_vertices;

        u32 lod_count = 0;
        raw_lod lod_array[shader_constants::kLODCount];
    };

    struct raw_primitive_data
    {
        cpp::heap_array<u32> raw_indices;
        cpp::heap_array<raw_vertex> raw_vertices;
    };

    // loads primitive + builds LODs
    raw_mesh build_mesh(const cgltf_primitive& prim);

    raw_primitive_data load_raw_primitive_data(const cgltf_primitive& prim);

    void build_meshlets(const cpp::heap_array<raw_vertex>& vertices, const cpp::heap_array<u32>& indices,
                        cpp::heap_array<raw_meshlet>& meshlets, cpp::heap_array<u8>& meshlets_payload) noexcept;
}
