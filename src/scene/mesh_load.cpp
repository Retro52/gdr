#include <cgltf.h>
#include <meshoptimizer.h>
#include <scene/mesh_load.hpp>

#include "cpp/containers/stack_string.hpp"

mesh::raw_mesh mesh::build_mesh(const cgltf_primitive& prim)
{
    ZoneScoped;

    mesh::raw_mesh result;
    cpp::heap_array<u32> indices_work_copy;

    {
        auto loaded_raw_prim_data = load_raw_primitive_data(prim);
        result.raw_vertices       = std::move(loaded_raw_prim_data.raw_vertices);
        indices_work_copy         = std::move(loaded_raw_prim_data.raw_indices);
    }

    auto& vertices      = result.raw_vertices;
    const f32 lod_scale = meshopt_simplifyScale(&vertices[0].position.x, vertices.size(), sizeof(vertices[0]));

    f32 curr_error = 0.0F;
    for (u32 j = 0; j < COUNT_OF(result.lod_array); ++j)
    {
        constexpr f32 kSimplifyMaxError         = 0.1F;
        constexpr f32 kSimplifyAttribWeights[]  = {1.0F, 1.0F, 1.0F};
        constexpr unsigned int kSimplifyOptions = meshopt_SimplifySparse;

        result.lod_array[j].raw_indices = indices_work_copy;
        build_meshlets(vertices,
                       result.lod_array[j].raw_indices,
                       result.lod_array[j].raw_meshlets,
                       result.lod_array[j].raw_meshlets_payload);

        ++result.lod_count;
        auto& curr_lod = result.lod_array[j];

        curr_lod.error = curr_error * lod_scale;

        if (j == COUNT_OF(result.lod_array) - 1)
        {
            break;
        }

        const u64 indices_target_count = (static_cast<u64>(static_cast<f64>(indices_work_copy.size()) * 0.6) / 3) * 3;

        f32 lod_error           = 0.f;
        const u64 indices_count = meshopt_simplifyWithAttributes(indices_work_copy.data(),
                                                                 indices_work_copy.data(),
                                                                 indices_work_copy.size(),
                                                                 &vertices[0].position.x,
                                                                 vertices.size(),
                                                                 sizeof(vertices[0]),
                                                                 &vertices[0].normal.x,
                                                                 sizeof(vertices[0]),
                                                                 kSimplifyAttribWeights,
                                                                 COUNT_OF(kSimplifyAttribWeights),
                                                                 nullptr,
                                                                 indices_target_count,
                                                                 kSimplifyMaxError,
                                                                 kSimplifyOptions,
                                                                 &lod_error);

        assert2(indices_count <= indices_work_copy.size());
        if (indices_count == indices_work_copy.size() || indices_count == 0
            || indices_count > (indices_work_copy.size() * 4 / 5))
        {
            break;
        }

        indices_work_copy.resize(indices_count);
        curr_error = lod_error;
        meshopt_optimizeVertexCache(
            indices_work_copy.data(), indices_work_copy.data(), indices_work_copy.size(), vertices.size());
    }

    return result;
}

mesh::raw_primitive_data mesh::load_raw_primitive_data(const cgltf_primitive& prim)
{
    ZoneScoped;

    // for (int i = 0; i < prim.attributes_count; ++i)
    constexpr int kAttrIdx = 0;
    const u64 vtx_count    = prim.attributes[kAttrIdx].data->count;

    cpp::heap_array<raw_vertex> raw_vertices(vtx_count);
    cpp::heap_array<cgltf_float> scratch(vtx_count * 4);

    auto unpack = [&](const cgltf_attribute_type attr, const u32 offset, const u32 expected_elem_count)
    {
        ZoneScopedN("unpack attribute");

        if (const auto* accessor = cgltf_find_accessor(&prim, attr, kAttrIdx))
        {
            const u64 count = cgltf_accessor_unpack_floats(accessor, scratch.data(), vtx_count * expected_elem_count);
            assert2(count == vtx_count * expected_elem_count);

            for (u64 j = 0; j < vtx_count; ++j)
            {
                cpp::cx_memcpy(reinterpret_cast<u8*>(&raw_vertices[j]) + offset,
                               &scratch[j * expected_elem_count],
                               sizeof(cgltf_float) * expected_elem_count);
            }
        }
    };

    unpack(cgltf_attribute_type_position, offsetof(raw_vertex, position), 3);
    unpack(cgltf_attribute_type_normal, offsetof(raw_vertex, normal), 3);
    unpack(cgltf_attribute_type_texcoord, offsetof(raw_vertex, uv), 2);
    unpack(cgltf_attribute_type_tangent, offsetof(raw_vertex, tangent), 4);

    cpp::heap_array<u32> indices(prim.indices->count);

    {
        ZoneScopedN("unpack indices");
        cgltf_accessor_unpack_indices(prim.indices, indices.data(), 4, indices.size());
    }

    u64 vertex_count = 0;
    cpp::heap_array<u32> remap(raw_vertices.size());

    {
        ZoneScopedN("generate remap");
        vertex_count = meshopt_generateVertexRemap(
            remap.data(), indices.data(), indices.size(), raw_vertices.data(), raw_vertices.size(), sizeof(raw_vertex));
    }

    cpp::heap_array<raw_vertex> vertices(vertex_count);

    {
        ZoneScopedN("apply remap");
        meshopt_remapVertexBuffer(
            vertices.data(), raw_vertices.data(), raw_vertices.size(), sizeof(raw_vertex), remap.data());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
    }

    {
        ZoneScopedN("optimize vertex cache");
        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
    }

    {
        ZoneScopedN("optimize vertex fetch");
        meshopt_optimizeVertexFetch(
            vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(vertices[0]));
    }

    return {.raw_indices = indices, .raw_vertices = vertices};
}

void mesh::build_meshlets(const cpp::heap_array<raw_vertex>& vertices, const cpp::heap_array<u32>& indices,
                          cpp::heap_array<raw_meshlet>& meshlets, cpp::heap_array<u8>& meshlets_payload) noexcept
{
    ZoneScoped;
    const u64 meshlets_upper_bound = meshopt_buildMeshletsBound(
        indices.size(), shader_constants::kMaxVerticesPerMeshlet, shader_constants::kMaxTrianglesPerMeshlet);

    const u64 vertices_offset = indices.size();
    cpp::heap_array<u8> meshlets_data(indices.size() * 5);

    u8* meshlet_indices_ptr   = meshlets_data.data();
    u32* meshlet_vertices_ptr = reinterpret_cast<u32*>(meshlets_data.data() + vertices_offset);

    cpp::heap_array<meshopt_Meshlet> meshopt_meshlets(meshlets_upper_bound);

    u64 meshlets_count = 0;
    {
        ZoneScopedN("generate meshlets");
        meshlets_count = meshopt_buildMeshlets(meshopt_meshlets.data(),
                                               meshlet_vertices_ptr,
                                               meshlet_indices_ptr,
                                               indices.data(),
                                               indices.size(),
                                               &vertices.data()->position.x,
                                               vertices.size(),
                                               sizeof(vertices[0]),
                                               shader_constants::kMaxVerticesPerMeshlet,
                                               shader_constants::kMaxTrianglesPerMeshlet,
                                               0.5F);
    }

    {
        ZoneScopedN("re-align meshlets array");
        constexpr u32 kTSAlign = shader_constants::kTaskWorkGroups;
        meshlets.resize(((meshlets_count + kTSAlign - 1) / kTSAlign) * kTSAlign);
    }

    {
        ZoneScopedN("encode meshlets");

        u64 total_bytes_written = 0;
        meshlets_payload.resize(meshlets_count * shader_constants::kMaxIndicesPerMeshlet
                                + meshlets_count * sizeof(u32) * shader_constants::kMaxVerticesPerMeshlet);

        for (u32 i = 0; i < meshlets_count; i++)
        {
            auto& meshopt_meshlet = meshopt_meshlets[i];

            meshopt_optimizeMeshlet(&meshlet_vertices_ptr[meshopt_meshlet.vertex_offset],
                                    &meshlet_indices_ptr[meshopt_meshlet.triangle_offset],
                                    meshopt_meshlet.triangle_count,
                                    meshopt_meshlet.vertex_count);

            meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshlet_vertices_ptr[meshopt_meshlet.vertex_offset],
                                                                 &meshlet_indices_ptr[meshopt_meshlet.triangle_offset],
                                                                 meshopt_meshlet.triangle_count,
                                                                 &vertices[0].position.x,
                                                                 vertices.size(),
                                                                 sizeof(vertices[0]));

            auto& meshlet           = meshlets[i];
            meshlet.data_prefix_sum = total_bytes_written;
            meshlet.triangles_count = meshopt_meshlet.triangle_count;
            meshlet.vertices_count  = meshopt_meshlet.vertex_count;

            cpp::cx_memcpy(meshlets_payload.data() + total_bytes_written,
                           &meshlet_vertices_ptr[meshopt_meshlet.vertex_offset],
                           meshlet.vertices_count * sizeof(u32));
            total_bytes_written += sizeof(u32) * meshlet.vertices_count;

            cpp::cx_memcpy(meshlets_payload.data() + total_bytes_written,
                           &meshlet_indices_ptr[meshopt_meshlet.triangle_offset],
                           meshlet.triangles_count * 3);
            total_bytes_written += meshlet.triangles_count * 3;

            // align data to 4 bytes to avoid payload overlaps between meshlets
            total_bytes_written = (total_bytes_written + 3) & ~3u;

            meshlet.cone_cutoff  = bounds.cone_cutoff;
            meshlet.cone_axis[0] = bounds.cone_axis[0];
            meshlet.cone_axis[1] = bounds.cone_axis[1];
            meshlet.cone_axis[2] = bounds.cone_axis[2];

            meshlet.sphere_radius    = bounds.radius;
            meshlet.sphere_center[0] = bounds.center[0];
            meshlet.sphere_center[1] = bounds.center[1];
            meshlet.sphere_center[2] = bounds.center[2];
        }

        for (u32 i = meshlets_count; i < meshlets.size(); i++)
        {
            meshlets[i].vertices_count  = 0;
            meshlets[i].triangles_count = 0;
        }

        // fit the array to compact the amount of data we upload to the GPU
        meshlets_payload.resize(total_bytes_written);
    }
}
