#include <meshoptimizer.h>
#include <render/platform/vk/vk_error.hpp>
#include <render/sm_cache.hpp>
#include <render/static_model.hpp>
#include <Tracy/Tracy.hpp>

#include <stack>

using namespace render;

namespace
{
    template<typename T>
    void upload_data(VmaAllocator allocator, const vk_buffer_transfer& transfer, vk_shared_buffer& dst_buffer,
                     const T* data, const u64 count)
    {
        ZoneScoped;

        void* mapped;
        vmaMapMemory(allocator, transfer.staging_buffer.allocation, &mapped);
        std::copy_n(
            reinterpret_cast<const u8*>(data), count * sizeof(T), (static_cast<u8*>(mapped)) + dst_buffer.offset);
        vmaUnmapMemory(allocator, transfer.staging_buffer.allocation);
        dst_buffer.offset += count * sizeof(T);

        VkCommandBufferBeginInfo begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        vkBeginCommandBuffer(transfer.staging_command_buffer.cmd_buffer, &begin_info);

        const VkBufferCopy copy_region {.size = count * sizeof(T)};
        vkCmdCopyBuffer(transfer.staging_command_buffer.cmd_buffer,
                        transfer.staging_buffer.buffer,
                        dst_buffer.buffer.buffer,
                        1,
                        &copy_region);

        vkEndCommandBuffer(transfer.staging_command_buffer.cmd_buffer);

        VkSubmitInfo submit_info {
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &transfer.staging_command_buffer.cmd_buffer,
        };

        vkQueueSubmit(transfer.queue.queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(transfer.queue.queue);
    }

#if SM_USE_MESHLETS
    void build_meshlets(const static_model::mesh_data& mesh, std::vector<static_model::static_model_meshlet>& meshlets,
                        std::vector<u8>& meshlet_indices, std::vector<u32>& meshlet_vertices, u32 base_meshlet) noexcept
    {
        ZoneScoped;
        const u64 meshlets_upper_bound = meshopt_buildMeshletsBound(
            mesh.indices.size(), static_model::kMaxVerticesPerMeshlet, static_model::kMaxTrianglesPerMeshlet);

        meshlet_indices.resize(mesh.indices.size());
        meshlet_vertices.resize(mesh.indices.size());

        std::vector<meshopt_Meshlet> meshopt_meshlets(meshlets_upper_bound);

        const u64 meshlets_count = meshopt_buildMeshlets(meshopt_meshlets.data(),
                                                         meshlet_vertices.data(),
                                                         meshlet_indices.data(),
                                                         mesh.indices.data(),
                                                         mesh.indices.size(),
                                                         &mesh.vertices.data()->position.x,
                                                         mesh.vertices.size(),
                                                         sizeof(static_model::static_model_vertex),
                                                         static_model::kMaxVerticesPerMeshlet,
                                                         static_model::kMaxTrianglesPerMeshlet,
                                                         0.5F);

#if SM_USE_TS
        // FIXME: this degrades performance by quite a bit
        const u32 ts_wg = shader_constants::kTaskWorkGroups;
        meshlets.resize(((meshlets_count + ts_wg - 1) / ts_wg) * ts_wg);
#else
        meshlets.resize(meshlets_count);
#endif

        {
            ZoneScopedN("meshopt_optimizeMeshlet and data copy");

            for (u32 i = 0; i < meshlets_count; i++)
            {
                auto& meshopt_meshlet = meshopt_meshlets[i];

                meshopt_optimizeMeshlet(&meshlet_vertices[meshopt_meshlet.vertex_offset],
                                        &meshlet_indices[meshopt_meshlet.triangle_offset],
                                        meshopt_meshlet.triangle_count,
                                        meshopt_meshlet.vertex_count);

                meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshlet_vertices[meshopt_meshlet.vertex_offset],
                                                                     &meshlet_indices[meshopt_meshlet.triangle_offset],
                                                                     meshopt_meshlet.triangle_count,
                                                                     &mesh.vertices[0].position.x,
                                                                     mesh.vertices.size(),
                                                                     sizeof(static_model::static_model_vertex));

                auto& meshlet           = meshlets[i];
                meshlet.index_offset    = meshopt_meshlet.triangle_offset;
                meshlet.vertex_offset   = meshopt_meshlet.vertex_offset;

                meshlet.triangles_count = meshopt_meshlet.triangle_count;
                meshlet.vertices_count  = meshopt_meshlet.vertex_count;

#if 0
                meshlet.cull_cone[0] = bounds.cone_axis_s8[0];
                meshlet.cull_cone[1] = bounds.cone_axis_s8[1];
                meshlet.cull_cone[2] = bounds.cone_axis_s8[2];
                meshlet.cull_cone[3] = bounds.cone_cutoff_s8;
#else
                meshlet.cull_cone[0] = bounds.cone_axis[0];
                meshlet.cull_cone[1] = bounds.cone_axis[1];
                meshlet.cull_cone[2] = bounds.cone_axis[2];
                meshlet.cull_cone[3] = bounds.cone_cutoff;
#endif
                meshlet.bounding_sphere[0] = bounds.cone_apex[0];
                meshlet.bounding_sphere[1] = bounds.cone_apex[1];
                meshlet.bounding_sphere[2] = bounds.cone_apex[2];
                meshlet.bounding_sphere[3] = bounds.radius;
            }
        }
    }
#endif

    void upload_to_scene(const static_model::mesh_data& mesh,
#if SM_USE_MESHLETS
                         const std::vector<static_model::static_model_meshlet>& meshlets,
                         const std::vector<u8>& meshlet_indices, const std::vector<u32>& meshlet_vertices,
#endif
                         const vk_renderer& renderer, vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

#if SM_USE_MESHLETS
        const auto allocator = renderer.get_context().allocator;
        upload_data(
            allocator, geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());
        upload_data(allocator, geometry_pool.transfer, geometry_pool.meshlets, meshlets.data(), meshlets.size());

        upload_data(allocator,
                    geometry_pool.transfer,
                    geometry_pool.meshlets_indices,
                    meshlet_indices.data(),
                    meshlet_indices.size());

        upload_data(allocator,
                    geometry_pool.transfer,
                    geometry_pool.meshlets_vertices,
                    meshlet_vertices.data(),
                    meshlet_vertices.size());
#else
        const auto allocator = renderer.get_context().allocator;
        upload_data(allocator, geometry_pool.transfer, geometry_pool.index, mesh.indices.data(), mesh.indices.size());
        upload_data(
            allocator, geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());
#endif
    }
}

result<static_model> static_model::load_model(const fs::path& path, render::vk_renderer& renderer,
                                              render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;

    stats model_stats {};
    std::vector<mesh_data> model_meshes;
    if (render::load_model<static_model_vertex>(path, model_meshes))
    {
#if SM_USE_MESHLETS
        ZoneScopedN("upload meshlets data");

        for (auto& mesh : model_meshes)
        {
            std::vector<u8> meshlet_indices;
            std::vector<u32> meshlet_vertices;
            std::vector<static_model_meshlet> meshlets;

            build_meshlets(mesh, meshlets, meshlet_indices, meshlet_vertices, model_stats.meshlets_count);

            model_stats.vertex_count += mesh.vertices.size();
            model_stats.meshlets_count += meshlets.size();
            for (auto& meshlet : meshlets)
            {
                model_stats.index_count += meshlet.triangles_count * 3;
            }

            upload_to_scene(mesh, meshlets, meshlet_indices, meshlet_vertices, renderer, geometry_pool);
            break;
        }
#else
        ZoneScopedN("static_model::load_model: upload data");

        for (auto& mesh : model_meshes)
        {
            model_stats.index_count += mesh.indices.size();
            model_stats.vertex_count += mesh.vertices.size();

            upload_to_scene(mesh, renderer, geometry_pool);
        }
#endif
        return static_model {model_stats};
    }

    return "failed to parse model";
}

void static_model::draw(VkCommandBuffer buffer) const
{
    ZoneScoped;

#if SM_USE_MESHLETS
#if SM_USE_TS
    vkCmdDrawMeshTasksEXT(buffer, m_stats.meshlets_count / shader_constants::kTaskWorkGroups, 1, 1);
#else
    vkCmdDrawMeshTasksEXT(buffer, m_stats.meshlets_count, 1, 1);
#endif
#else
    vkCmdDrawIndexed(buffer, m_stats.index_count, 1, 0, 0, 0);
#endif
}

u32 static_model::indices_count() const
{
    return m_stats.index_count;
}

[[nodiscard]] u32 static_model::meshlets_count() const
{
    return m_stats.meshlets_count;
}

static_model::static_model(const stats& stats)
    : m_stats(stats)
{
}
