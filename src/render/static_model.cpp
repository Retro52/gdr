#include <meshoptimizer.h>
#include <render/platform/vk/vk_error.hpp>
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

#if SM_USE_MESHLETS
    void build_meshlets(const static_model::mesh_data& mesh, std::vector<static_model::meshlet>& meshlets,
                        std::vector<u8>& meshlets_payload, u32 base_payload_offset) noexcept
    {
        ZoneScoped;
        const u64 meshlets_upper_bound = meshopt_buildMeshletsBound(
            mesh.indices.size(), static_model::kMaxVerticesPerMeshlet, static_model::kMaxTrianglesPerMeshlet);

        const u64 vertices_offset = mesh.indices.size();
        std::vector<u8> meshlets_data(mesh.indices.size() * 5);

        u8* meshlet_indices_ptr   = meshlets_data.data();
        u32* meshlet_vertices_ptr = reinterpret_cast<u32*>(meshlets_data.data() + vertices_offset);

        std::vector<meshopt_Meshlet> meshopt_meshlets(meshlets_upper_bound);

        const u64 meshlets_count = meshopt_buildMeshlets(meshopt_meshlets.data(),
                                                         meshlet_vertices_ptr,
                                                         meshlet_indices_ptr,
                                                         mesh.indices.data(),
                                                         mesh.indices.size(),
                                                         &mesh.vertices.data()->position.x,
                                                         mesh.vertices.size(),
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
                                                 &mesh.vertices[0].position.x,
                                                 mesh.vertices.size(),
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

            // fit the array to compact the amount of data we upload to the GPU
            meshlets_payload.resize(total_bytes_written);
        }
    }
#endif

    void upload_to_scene(const static_model::mesh_data& mesh,
#if SM_USE_MESHLETS
                         const std::vector<static_model::meshlet>& meshlets, const std::vector<u8>& meshlets_payload,
#endif
                         vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

#if SM_USE_MESHLETS
        upload_data(geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());
        upload_data(geometry_pool.transfer, geometry_pool.meshlets, meshlets.data(), meshlets.size());

        upload_data(
            geometry_pool.transfer, geometry_pool.meshlets_payload, meshlets_payload.data(), meshlets_payload.size());
#else
        upload_data(geometry_pool.transfer, geometry_pool.index, mesh.indices.data(), mesh.indices.size());
        upload_data(geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());
#endif
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
#if SM_USE_MESHLETS
        for (u32 i = 0; i < model_meshes.size(); ++i)
        {
            std::vector<meshlet> meshlets;
            std::vector<u8> meshlets_payload;

            build_meshlets(model_meshes[i], meshlets, meshlets_payload, geometry_pool.meshlets_payload.offset);

            models[i].meshlets_count = meshlets.size();
            models[i].b_sphere       = compute_bounding_sphere(model_meshes[i]);

            assert(geometry_pool.vertex.offset % sizeof(vertex) == 0);
            assert(geometry_pool.meshlets.offset % sizeof(meshlet) == 0);

            models[i].base_vertex  = geometry_pool.vertex.offset / sizeof(vertex);
            models[i].base_meshlet = geometry_pool.meshlets.offset / sizeof(meshlet);
            upload_to_scene(model_meshes[i], meshlets, meshlets_payload, geometry_pool);
        }
#else
        for (u32 i = 0; i < model_meshes.size(); ++i)
        {
            models[i].meshlets_count = model_meshes[i].indices.size();
            models[i].base_vertex    = geometry_pool.vertex.offset / sizeof(vertex);
            models[i].base_meshlet   = geometry_pool.index.offset / sizeof(u32);

            models[i].b_sphere = compute_bounding_sphere(model_meshes[i]);
            upload_to_scene(model_meshes[i], geometry_pool);
        }
#endif
        return models;
    }

    return "failed to parse the model";
}
