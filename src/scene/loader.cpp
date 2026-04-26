#include <assert2.hpp>
#include <cgltf.h>
#include <janitor.hpp>
#include <meshoptimizer.h>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/loader.hpp>

#include "glm/gtc/type_ptr.inl"
#include "scene.hpp"

#define CHECK(EXPR)                   \
    if (EXPR != cgltf_result_success) \
    {                                 \
        assert2m(false, #EXPR);       \
        return {};                    \
    }

namespace
{
    // TODO: batch data uploads together
    template<typename T>
    void upload_data(const render::vk_buffer_transfer& transfer, render::vk_shared_buffer& dst_buffer, const T* data,
                     const u64 count)
    {
        ZoneScoped;

        render::upload_data(transfer,
                            dst_buffer.buffer,
                            reinterpret_cast<const u8*>(data),
                            VkBufferCopy {.srcOffset = 0, .dstOffset = dst_buffer.offset, .size = count * sizeof(T)});
        dst_buffer.offset += count * sizeof(T);
    }

    loader::mesh_data load_mesh(const cgltf_primitive& prim) noexcept
    {
        ZoneScoped;

        // for (int i = 0; i < prim.attributes_count; ++i)
        constexpr int kAttrIdx = 0;
        const u64 vtx_count    = prim.attributes[kAttrIdx].data->count;
        cpp::heap_array<loader::vertex> raw_vertices(vtx_count);

        cpp::heap_array<cgltf_float> scratch(vtx_count * 3);

        if (const auto* accessor = cgltf_find_accessor(&prim, cgltf_attribute_type_position, kAttrIdx))
        {
            const u64 count = cgltf_accessor_unpack_floats(accessor, scratch.data(), vtx_count * 3);

            assert2(count == vtx_count * 3);
            for (u64 j = 0; j < vtx_count; ++j)
            {
                raw_vertices[j].position = {scratch[j * 3 + 0], scratch[j * 3 + 1], scratch[j * 3 + 2]};
            }
        }

        if (const cgltf_accessor* accessor = cgltf_find_accessor(&prim, cgltf_attribute_type_normal, kAttrIdx))
        {
            assert2(cgltf_num_components(accessor->type) == 3);
            const u64 count = cgltf_accessor_unpack_floats(accessor, scratch.data(), vtx_count * 3);

            assert2(count == vtx_count * 3);
            for (size_t j = 0; j < vtx_count; ++j)
            {
                raw_vertices[j].normal = {scratch[j * 3 + 0], scratch[j * 3 + 1], scratch[j * 3 + 2]};
            }
        }

        cpp::heap_array<u32> indices(prim.indices->count);
        cgltf_accessor_unpack_indices(prim.indices, indices.data(), 4, indices.size());

        u64 vertex_count = 0;
        cpp::heap_array<u32> remap(raw_vertices.size());

        {
            ZoneScopedN("meshopt_generateVertexRemap");

            vertex_count = meshopt_generateVertexRemap(remap.data(),
                                                       indices.data(),
                                                       indices.size(),
                                                       raw_vertices.data(),
                                                       raw_vertices.size(),
                                                       sizeof(loader::vertex));
        }

        cpp::heap_array<loader::vertex> vertices(vertex_count);

        {
            ZoneScopedN("meshopt_remap[Vertex/Index]Buffer");

            meshopt_remapVertexBuffer(
                vertices.data(), raw_vertices.data(), raw_vertices.size(), sizeof(loader::vertex), remap.data());
            meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
        }

        {
            ZoneScopedN("meshopt_optimizeVertexCache");
            meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
        }

        {
            ZoneScopedN("meshopt_optimizeVertexFetch");
            meshopt_optimizeVertexFetch(vertices.data(),
                                        indices.data(),
                                        indices.size(),
                                        vertices.data(),
                                        vertices.size(),
                                        sizeof(loader::vertex));
        }

        return {.vertices = vertices, .indices = indices};
    }

    void build_meshlets(const cpp::heap_array<loader::vertex>& vertices, const cpp::heap_array<u32>& indices,
                        cpp::heap_array<loader::meshlet>& meshlets, cpp::heap_array<u8>& meshlets_payload,
                        const u32 base_payload_offset) noexcept
    {
        ZoneScoped;
        const u64 meshlets_upper_bound =
            meshopt_buildMeshletsBound(indices.size(), loader::kMaxVerticesPerMeshlet, loader::kMaxTrianglesPerMeshlet);

        const u64 vertices_offset = indices.size();
        cpp::heap_array<u8> meshlets_data(indices.size() * 5);

        u8* meshlet_indices_ptr   = meshlets_data.data();
        u32* meshlet_vertices_ptr = reinterpret_cast<u32*>(meshlets_data.data() + vertices_offset);

        cpp::heap_array<meshopt_Meshlet> meshopt_meshlets(meshlets_upper_bound);

        const u64 meshlets_count = meshopt_buildMeshlets(meshopt_meshlets.data(),
                                                         meshlet_vertices_ptr,
                                                         meshlet_indices_ptr,
                                                         indices.data(),
                                                         indices.size(),
                                                         &vertices.data()->position.x,
                                                         vertices.size(),
                                                         sizeof(loader::vertex),
                                                         loader::kMaxVerticesPerMeshlet,
                                                         loader::kMaxTrianglesPerMeshlet,
                                                         0.5F);

        constexpr u32 kTSAlign = shader_constants::kTaskWorkGroups;
        meshlets.resize(((meshlets_count + kTSAlign - 1) / kTSAlign) * kTSAlign);

        {
            ZoneScopedN("meshopt_optimizeMeshlet and data copy");

            u64 total_bytes_written = 0;
            meshlets_payload.resize(meshlets_count * loader::kMaxIndicesPerMeshlet
                                    + meshlets_count * sizeof(u32) * loader::kMaxVerticesPerMeshlet);

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
                                                 sizeof(loader::vertex));

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
                meshlets[i].vertices_count  = 0;
                meshlets[i].triangles_count = 0;
            }

            // fit the array to compact the amount of data we upload to the GPU
            meshlets_payload.resize(total_bytes_written);
        }
    }

    void generate_lods(const loader::mesh_data& data, loader::primitive& primitive,
                       render::vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

        cpp::heap_array<u8> meshlets_payload;
        cpp::heap_array<loader::meshlet> meshlets;

        cpp::heap_array<u32> indices_work_copy = data.indices;
        const f32 lod_scale =
            meshopt_simplifyScale(&data.vertices[0].position.x, data.vertices.size(), sizeof(loader::vertex));

        f32 curr_error = 0.0F;
        for (u32 j = 0; j < COUNT_OF(primitive.lod_array); ++j)
        {
            constexpr f32 kSimplifyMaxError         = 0.1F;
            constexpr f32 kSimplifyAttribWeights[]  = {1.0F, 1.0F, 1.0F};
            constexpr unsigned int kSimplifyOptions = meshopt_SimplifySparse;

            build_meshlets(
                data.vertices, indices_work_copy, meshlets, meshlets_payload, geometry_pool.meshlets_payload.offset);

            ++primitive.lod_count;
            auto& curr_lod = primitive.lod_array[j];

            curr_lod.lod_error = curr_error * lod_scale;

            assert2(geometry_pool.index.offset % sizeof(u32) == 0);
            assert2(geometry_pool.meshlets.offset % sizeof(loader::meshlet) == 0);

            curr_lod.meshlets_count = meshlets.size();
            curr_lod.base_meshlet   = geometry_pool.meshlets.offset / sizeof(loader::meshlet);

            curr_lod.indices_count = indices_work_copy.size();
            curr_lod.base_index    = geometry_pool.index.offset / sizeof(u32);

            upload_data(geometry_pool.transfer, geometry_pool.meshlets, meshlets.data(), meshlets.size());
            upload_data(
                geometry_pool.transfer, geometry_pool.index, indices_work_copy.data(), indices_work_copy.size());
            upload_data(geometry_pool.transfer,
                        geometry_pool.meshlets_payload,
                        meshlets_payload.data(),
                        meshlets_payload.size());

            if (j == COUNT_OF(primitive.lod_array) - 1)
            {
                break;
            }

            const u64 indices_target_count =
                (static_cast<u64>(static_cast<f64>(indices_work_copy.size()) * 0.6) / 3) * 3;

            f32 lod_error           = 0.f;
            const u64 indices_count = meshopt_simplifyWithAttributes(indices_work_copy.data(),
                                                                     indices_work_copy.data(),
                                                                     indices_work_copy.size(),
                                                                     &data.vertices[0].position.x,
                                                                     data.vertices.size(),
                                                                     sizeof(data.vertices[0]),
                                                                     &data.vertices[0].normal.x,
                                                                     sizeof(data.vertices[0]),
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
            curr_error = std::max(curr_error * 1.5F, lod_error);
            meshopt_optimizeVertexCache(
                indices_work_copy.data(), indices_work_copy.data(), indices_work_copy.size(), data.vertices.size());
        }
    }

    vec4 compute_bounding_sphere(const loader::mesh_data& mesh)
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
}

loader::stats loader::load(const fs::path& path, scene& scene, render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;
    if (path.extension() != ".gltf" && path.extension() != ".glb")
    {
        return {};
    }

    cgltf_options options = {};
    cgltf_data* data      = nullptr;

    CHECK(cgltf_parse_file(&options, path.c_str(), &data));
    SUMMON_JANITOR(cgltf_free(data));

    CHECK(cgltf_load_buffers(&options, data, path.c_str()));
    CHECK(cgltf_validate(data));

    loader::stats stats {
        .primitives = geometry_pool.primitives.offset / sizeof(loader::primitive),
        .triangles  = geometry_pool.index.offset / sizeof(u32),
    };

    cpp::heap_array<loader::primitive> primitive_data;
    cpp::heap_array<loader::mesh_desc> meshes(data->meshes_count);

    for (u64 i = 0; i < data->meshes_count; ++i)
    {
        const cgltf_mesh& mesh = data->meshes[i];

        u32 first_primitive = primitive_data.size();
        for (u64 prim = 0; prim < mesh.primitives_count; ++prim)
        {
            const auto& primitive = mesh.primitives[prim];
            if (primitive.type != cgltf_primitive_type_triangles || !primitive.indices || !primitive.attributes)
            {
                continue;
            }

            auto prim_data = load_mesh(primitive);

            auto& mesh_data       = primitive_data.emplace_back();
            mesh_data.b_sphere    = compute_bounding_sphere(prim_data);
            mesh_data.base_vertex = geometry_pool.vertex.offset / sizeof(vertex);

            assert2(geometry_pool.vertex.offset % sizeof(vertex) == 0);
            upload_data(
                geometry_pool.transfer, geometry_pool.vertex, prim_data.vertices.data(), prim_data.vertices.size());

            generate_lods(prim_data, mesh_data, geometry_pool);
        }

        meshes[i] = {.offset     = first_primitive,
                     .prim_count = static_cast<u32>(primitive_data.size() - first_primitive)};
    }

    upload_data(geometry_pool.transfer, geometry_pool.primitives, primitive_data.data(), primitive_data.size());

    stats = {
        .meshes     = meshes.size(),
        .primitives = geometry_pool.primitives.offset / sizeof(loader::primitive) - stats.primitives,
        .triangles  = (geometry_pool.index.offset / sizeof(u32) - stats.triangles) / 3,
    };

#if !defined(NDEBUG)
    auto& hierarchy = scene.hierarchy;
    hierarchy.nodes.reserve(data->nodes_count);
#endif

    // TODO: refactor (moving objects?)
    // Also need to be careful with not writing anything to transfer in the meantime
    u32 transforms_count = 0;
    auto* transforms     = static_cast<transform_component*>(geometry_pool.transfer.mapped);

    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        const cgltf_node* node = &data->nodes[i];
        auto entity            = scene.create_entity();
        entity.add_component<id_component>(DEBUG_ONLY(node->name ? node->name : "[undefined]"));

        glm::mat4 transform;
        cgltf_node_transform_world(node, glm::value_ptr(transform));

        auto& transform_comp = entity.emplace_component<transform_component>(transform);

        if (node->mesh)
        {
            auto& component = entity.emplace_component<mesh_component>();

            component.mesh_offset      = meshes[cgltf_mesh_index(data, node->mesh)].offset;
            component.primitives_count = meshes[cgltf_mesh_index(data, node->mesh)].prim_count;

            auto& desc       = meshes[cgltf_mesh_index(data, node->mesh)];
            transforms_count = cpp::max(desc.offset + desc.prim_count - 1, transforms_count);

            for (u32 j = 0; j < desc.prim_count; ++j)
            {
                transforms[desc.offset + j] = transform_comp;
            }
        }

        if (node->camera)
        {
            auto& component = entity.emplace_component<camera_component>();

            component.horizontal_fov = node->camera->data.perspective.yfov;
            component.near_plane     = node->camera->data.perspective.znear;
            component.aspect_ratio   = node->camera->data.perspective.aspect_ratio;
        }

#if !defined(NDEBUG)
        auto& scene_node = hierarchy.nodes.emplace_back();
        scene_node.e     = entity.get_native();
        if (node->parent)
        {
            scene_node.parent = cgltf_node_index(data, node->parent);
            scene_node.children.reserve(node->children_count);
            for (size_t c = 0; c < node->children_count; ++c)
            {
                scene_node.children.push_back(static_cast<u32>(cgltf_node_index(data, node->children[c])));
            }
        }
        else
        {
            hierarchy.roots.emplace_back(cgltf_node_index(data, node));
        }
#endif
    }

    render::submit_transfer(geometry_pool.transfer,
                            geometry_pool.transforms.buffer,
                            VkBufferCopy {.size = transforms_count * sizeof(transform_component)});
    geometry_pool.transforms.offset += transforms_count * sizeof(transform_component);
    // TODO

    return stats;
}
