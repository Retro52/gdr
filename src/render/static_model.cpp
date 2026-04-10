#include <assert2.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cpp/alg_constexpr.hpp>
#include <fs/fs.hpp>
#include <glm/geometric.hpp>
#include <meshoptimizer.h>
#include <render/static_model.hpp>
#include <tracy/Tracy.hpp>

#include <stack>

using namespace render;

namespace
{
    // TODO: waiting here (again) caching system rewrite w/ compression and other stuff. Coming, probably, never tbh

    static_model::mesh_data load_mesh(const aiMesh* mesh) noexcept
    {
        ZoneScoped;
        assert2(mesh->HasNormals());

        cpp::heap_array<static_model::vertex> raw_vertices(mesh->mNumVertices);
        for (u32 i = 0; i < mesh->mNumVertices; i++)
        {
            if (mesh->mTextureCoords[0]) [[likely]]
            {
                raw_vertices[i].position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
                raw_vertices[i].normal   = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
#if 0
            raw_vertices[i].uv       = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
            raw_vertices[i].tangent  = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
#endif
            }
            else
            {
                raw_vertices[i].position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
                raw_vertices[i].normal   = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
            }
        }

        // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding
        // vertex indices.
        // aiProcess_Triangulate guarantees every face will only contain triangles, except line/point faces,
        // but we don't care about those
        cpp::heap_array<u32> indices(mesh->mNumFaces * 3);
        for (u32 i = 0; i < mesh->mNumFaces; i++)
        {
            const u32 base = i * 3;
            const u32* src = mesh->mFaces[i].mIndices;
            assert2(mesh->mFaces[i].mNumIndices == 3);

            indices[base + 0] = src[0];
            indices[base + 1] = src[1];
            indices[base + 2] = src[2];
        }

        u64 vertex_count = 0;
        cpp::heap_array<u32> remap(indices.size());

        {
            ZoneScopedN("meshopt_generateVertexRemap");

            vertex_count = meshopt_generateVertexRemap(remap.data(),
                                                       indices.data(),
                                                       indices.size(),
                                                       raw_vertices.data(),
                                                       raw_vertices.size(),
                                                       sizeof(static_model::vertex));
        }

        cpp::heap_array<static_model::vertex> vertices(vertex_count);

        {
            ZoneScopedN("meshopt_remap[Vertex/Index]Buffer");

            meshopt_remapVertexBuffer(
                vertices.data(), raw_vertices.data(), raw_vertices.size(), sizeof(static_model::vertex), remap.data());
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
                                        sizeof(static_model::vertex));
        }

        return {.vertices = vertices, .indices = indices};
    }

    result<cpp::heap_array<static_model::mesh_data>> parse_model(const fs::path& path)
    {
        ZoneScoped;

        auto contents = fs::read_file(path);
        if (!contents)
        {
            return error(contents.message);
        }

        const auto& data = contents.value;

        const aiScene* scene = nullptr;
        cpp::heap_array<static_model::mesh_data> meshes;

        {
            ZoneScopedN("Assimp::ReadFileFromMemory");

#if 1
            thread_local Assimp::Importer importer;
#else
            Assimp::Importer importer;
#endif

            constexpr auto flags =
                aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace;
            scene = importer.ReadFileFromMemory(data.data(), data.size(), flags);

            if ((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0)
                || (scene->mRootNode == nullptr))
            {
                return error(importer.GetErrorString());
            }
        }

        {
            std::stack<aiNode*> process_nodes;
            ZoneScopedN("process sub-meshes");

            process_nodes.push(scene->mRootNode);

            while (!process_nodes.empty())
            {
                const auto* node = process_nodes.top();
                process_nodes.pop();

                for (unsigned int i = 0; i < node->mNumMeshes; i++)
                {
                    // the node object only contains indices to index the actual objects in the scene.
                    // the scene contains all the data, node is just to keep stuff organized (like relations between
                    // nodes).
                    const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                    meshes.push_back(load_mesh(mesh));
                }

                for (unsigned int i = 0; i < node->mNumChildren; i++)
                {
                    process_nodes.push(node->mChildren[i]);
                }
            }
        }

        return meshes;
    }

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

    void build_meshlets(const cpp::heap_array<static_model::vertex>& vertices, const cpp::heap_array<u32>& indices,
                        cpp::heap_array<static_model::meshlet>& meshlets, cpp::heap_array<u8>& meshlets_payload,
                        const u32 base_payload_offset) noexcept
    {
        ZoneScoped;
        const u64 meshlets_upper_bound = meshopt_buildMeshletsBound(
            indices.size(), static_model::kMaxVerticesPerMeshlet, static_model::kMaxTrianglesPerMeshlet);

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
                meshlets[i].vertices_count  = 0;
                meshlets[i].triangles_count = 0;
            }

            // fit the array to compact the amount of data we upload to the GPU
            meshlets_payload.resize(total_bytes_written);
        }
    }
}

result<cpp::heap_array<static_model>> static_model::load(const fs::path& path,
                                                         render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;

    if (auto result = parse_model(path))
    {
        const cpp::heap_array<mesh_data>& model_meshes = *result;
        cpp::heap_array<static_model> models(model_meshes.size());

        cpp::heap_array<meshlet> meshlets;
        cpp::heap_array<u8> meshlets_payload;

        for (u32 i = 0; i < model_meshes.size(); ++i)
        {
            auto& mesh  = model_meshes[i];
            auto& model = models[i];

            models[i].b_sphere = compute_bounding_sphere(mesh);

            assert2(geometry_pool.vertex.offset % sizeof(vertex) == 0);
            models[i].base_vertex = geometry_pool.vertex.offset / sizeof(vertex);
            upload_data(geometry_pool.transfer, geometry_pool.vertex, mesh.vertices.data(), mesh.vertices.size());

            cpp::heap_array<u32> indices_work_copy = mesh.indices;
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
                if (indices_count == indices_work_copy.size() || indices_count == 0
                    || indices_count > (indices_work_copy.size() * 4 / 5))
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

    return error("failed to parse the model");
}
