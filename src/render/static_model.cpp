#include <assert2.hpp>
#include <meshoptimizer.h>
#include <render/sm_cache.hpp>
#include <render/static_model.hpp>
#include <Tracy/Tracy.hpp>

#include <stack>

using namespace render;

namespace
{
    // TODO: batch data uploads together
    template<typename T>
    void upload_data(const vk_buffer_transfer& transfer, vk_shared_buffer& dst_buffer, const T* data, const u64 count)
    {
        ZoneScoped;

        render::upload_data(transfer,
                            dst_buffer.buffer,
                            reinterpret_cast<const u8*>(data),
                            VkBufferCopy {.srcOffset = 0, .dstOffset = dst_buffer.offset, .size = count * sizeof(T)});
        dst_buffer.offset += count * sizeof(T);
    }

    vec4 compute_bounding_sphere(const static_model::mesh_data& mesh)
    {
        ZoneScoped;
        vec3 center(0.0F);
        for (const auto& v : mesh.vertices)
        {
            center += v.position;
        }

        f32 radius = 0.0F;
        center /= mesh.vertices.size();

        for (const auto& v : mesh.vertices)
        {
            radius = glm::max(radius, glm::distance(center, v.position));
        }

        return {center, radius};
    }

    void build_meshlets(const std::vector<static_model::vertex>& vertices, const std::vector<u32>& indices,
                        std::vector<static_model::meshlet>& meshlets, std::vector<u8>& meshlets_payload,
                        u32 base_payload_offset) noexcept
    {
        ZoneScoped;
        const u64 meshlets_upper_bound = meshopt_buildMeshletsBound(
            indices.size(), static_model::kMaxVerticesPerMeshlet, static_model::kMaxTrianglesPerMeshlet);

        const u64 vertices_offset = indices.size();
        std::vector<u8> meshlets_data(indices.size() * 5);

        u8* meshlet_indices_ptr   = meshlets_data.data();
        u32* meshlet_vertices_ptr = reinterpret_cast<u32*>(meshlets_data.data() + vertices_offset);

        std::vector<meshopt_Meshlet> meshopt_meshlets(meshlets_upper_bound);

        const u64 meshlets_count = meshopt_buildMeshlets(meshopt_meshlets.data(),
                                                         meshlet_vertices_ptr,
                                                         meshlet_indices_ptr,
                                                         indices.data(),
                                                         indices.size(),
                                                         &vertices.data()->position.x,
                                                         vertices.size(),
                                                         sizeof(static_model::vertex),
                                                         static_model::kMaxVerticesPerMeshlet,
                                                         static_model::kMaxTrianglesPerMeshlet,
                                                         0.5F);

        constexpr u32 kTSAlign = shader_constants::kTaskWorkGroups;
        meshlets.resize(((meshlets_count + kTSAlign - 1) / kTSAlign) * kTSAlign);

        {
            ZoneScopedN("meshopt_optimizeMeshlet and data copy");

            u64 total_bytes_written = 0;
            meshlets_payload.resize(meshlets_count * static_model::kMaxIndicesPerMeshlet
                                    + meshlets_count * sizeof(u32) * static_model::kMaxVerticesPerMeshlet);

            for (u32 i = 0; i < meshlets_count; i++)
            {
                auto& meshopt_meshlet = meshopt_meshlets[i];

                meshopt_optimizeMeshlet(&meshlet_vertices_ptr[meshopt_meshlet.vertex_offset],
                                        &meshlet_indices_ptr[meshopt_meshlet.triangle_offset],
                                        meshopt_meshlet.triangle_count,
                                        meshopt_meshlet.vertex_count);

                meshopt_Bounds bounds =
                    meshopt_computeMeshletBounds(&meshlet_vertices_ptr[meshopt_meshlet.vertex_offset],
                                                 &meshlet_indices_ptr[meshopt_meshlet.triangle_offset],
                                                 meshopt_meshlet.triangle_count,
                                                 &vertices[0].position.x,
                                                 vertices.size(),
                                                 sizeof(static_model::vertex));

                auto& meshlet           = meshlets[i];
                meshlet.triangles_count = meshopt_meshlet.triangle_count;
                meshlet.vertices_count  = meshopt_meshlet.vertex_count;
                meshlet.payload_offset  = base_payload_offset + total_bytes_written;

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
                meshlets[i].vertices_count = 0;
                meshlets[i].triangles_count = 0;
            }

            // fit the array to compact the amount of data we upload to the GPU
            meshlets_payload.resize(total_bytes_written);
        }
    }
}

result<std::vector<static_model>> static_model::load(const fs::path& path,
                                                     render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;

    std::vector<mesh_data> model_meshes;
    if (render::load_model<vertex>(path, model_meshes))
    {
        std::vector<static_model> models(model_meshes.size());

        std::vector<meshlet> meshlets;
        std::vector<u8> meshlets_payload;

        for (u32 i = 0; i < model_meshes.size(); ++i)
        {
            auto& mesh  = model_meshes[i];
            auto& model = models[i];

            models[i].b_sphere = compute_bounding_sphere(mesh);

            assert2(geometry_pool.vertex.offset % sizeof(vertex) == 0);
            models[i].base_vertex = geometry_pool.vertex.offset / sizeof(vertex);
            upload_data(geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());

            std::vector<u32> indices_work_copy = mesh.indices;
            const f32 lod_scale =
                meshopt_simplifyScale(&mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(vertex));

            f32 curr_error = 0.0F;
            for (u32 j = 0; j < COUNT_OF(lod_array); ++j)
            {
                constexpr f32 kSimplifyMaxError         = 0.1F;
                constexpr f32 kSimplifyAttribWeights[]  = {1.0F, 1.0F, 1.0F};
                constexpr unsigned int kSimplifyOptions = meshopt_SimplifySparse;

                build_meshlets(mesh.vertices,
                               indices_work_copy,
                               meshlets,
                               meshlets_payload,
                               geometry_pool.meshlets_payload.offset);

                ++model.lod_count;
                auto& curr_lod = model.lod_array[j];

                curr_lod.lod_error = curr_error * lod_scale;

                assert2(geometry_pool.index.offset % sizeof(u32) == 0);
                assert2(geometry_pool.meshlets.offset % sizeof(meshlet) == 0);

                curr_lod.meshlets_count = meshlets.size();
                curr_lod.base_meshlet   = geometry_pool.meshlets.offset / sizeof(meshlet);

                curr_lod.indices_count = indices_work_copy.size();
                curr_lod.base_index    = geometry_pool.index.offset / sizeof(u32);

                upload_data(geometry_pool.transfer, geometry_pool.meshlets, meshlets.data(), meshlets.size());
                upload_data(
                    geometry_pool.transfer, geometry_pool.index, indices_work_copy.data(), indices_work_copy.size());
                upload_data(geometry_pool.transfer,
                            geometry_pool.meshlets_payload,
                            meshlets_payload.data(),
                            meshlets_payload.size());

                if (j == COUNT_OF(lod_array) - 1)
                {
                    break;
                }

                const u64 indices_target_count =
                    (static_cast<u64>(static_cast<f64>(indices_work_copy.size()) * 0.6) / 3) * 3;

                f32 lod_error           = 0.f;
                const u64 indices_count = meshopt_simplifyWithAttributes(indices_work_copy.data(),
                                                                         indices_work_copy.data(),
                                                                         indices_work_copy.size(),
                                                                         &mesh.vertices[0].position.x,
                                                                         mesh.vertices.size(),
                                                                         sizeof(mesh.vertices[0]),
                                                                         &mesh.vertices[0].normal.x,
                                                                         sizeof(mesh.vertices[0]),
                                                                         kSimplifyAttribWeights,
                                                                         COUNT_OF(kSimplifyAttribWeights),
                                                                         nullptr,
                                                                         indices_target_count,
                                                                         kSimplifyMaxError,
                                                                         kSimplifyOptions,
                                                                         &lod_error);

                assert2(indices_count <= indices_work_copy.size());
                if (indices_count == indices_work_copy.size() || indices_count == 0)
                {
                    break;
                }

                indices_work_copy.resize(indices_count);
                curr_error = std::max(curr_error * 1.5F, lod_error);
                meshopt_optimizeVertexCache(
                    indices_work_copy.data(), indices_work_copy.data(), indices_work_copy.size(), mesh.vertices.size());
            }
        }
        return models;
    }

    return "failed to parse the model";
}
