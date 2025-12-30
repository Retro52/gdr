#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>
#include <render/platform/vk/vk_error.hpp>
#include <render/static_model.hpp>
#include <Tracy/Tracy.hpp>

using namespace render;

namespace
{
    // TODO: staging buffer
    template<typename T>
    void upload_data(const vk_renderer& renderer, const vk_buffer& buffer, const u64 offset, const T* data,
                     const u64 count)
    {
        ZoneScoped;
        const auto allocator = renderer.get_context().allocator;

        void* mapped;
        vmaMapMemory(allocator, buffer.allocation, &mapped);
        std::copy_n(
            reinterpret_cast<const u8*>(data), count * sizeof(T), (static_cast<u8*>(mapped)) + (offset * sizeof(T)));
        vmaUnmapMemory(allocator, buffer.allocation);
    }

    static_model::mesh_data load_mesh(const aiMesh* mesh) noexcept
    {
        ZoneScoped;
        assert(mesh->HasNormals());

        std::vector<static_model::static_model_vertex> raw_vertices(mesh->mNumVertices);
        for (u32 i = 0; i < mesh->mNumVertices; i++)
        {
            if (mesh->mTextureCoords[0]) [[likely]]
            {
                raw_vertices[i].position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
                raw_vertices[i].normal   = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
                raw_vertices[i].uv       = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
                raw_vertices[i].tangent  = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
            }
            else
            {
                raw_vertices[i].position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
                raw_vertices[i].normal   = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
            }
        }

        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding
        // vertex indices.
        // aiProcess_Triangulate guarantees every face will only contain triangles, except line/point faces,
        // but we don't care about those
        std::vector<u32> indices(mesh->mNumFaces * 3);
        for (u32 i = 0; i < mesh->mNumFaces; i++)
        {
            const u32 base = i * 3;
            const u32* src = mesh->mFaces[i].mIndices;
            assert(mesh->mFaces[i].mNumIndices == 3);

            indices[base + 0] = src[0];
            indices[base + 1] = src[1];
            indices[base + 2] = src[2];
        }

#if 0
        constexpr u32 kMaxVerticesPerMeshlet  = 64;
        constexpr u32 kMaxTrianglesPerMeshlet = 126;

        const u64 meshlets_upper_bound =
            meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);

        static_model::mesh_data mesh_data {
            .meshlets {meshlets_upper_bound},
        };

        const u64 meshlets_count = meshopt_buildMeshlets(mesh_data.meshlets.data(),
                                                         mesh_data.meshlet_vertices.data(),
                                                         mesh_data.meshlet_triangles.data(),
                                                         indices.data(),
                                                         indices.size(),
                                                         &vertices.data()->position.x,
                                                         vertices.size(),
                                                         sizeof(static_model::static_model_vertex),
                                                         kMaxVerticesPerMeshlet,
                                                         kMaxTrianglesPerMeshlet,
                                                         0.0F);

        mesh_data.meshlets.resize(meshlets_count);
        for (auto& meshlet : mesh_data.meshlets)
        {
            meshopt_optimizeMeshlet(&mesh_data.meshlet_vertices[meshlet.vertex_offset],
                                    &mesh_data.meshlet_triangles[meshlet.triangle_offset],
                                    meshlet.triangle_count,
                                    meshlet.vertex_count);
        }

        return mesh_data;
#else
        u64 vertex_count = 0;
        std::vector<u32> remap(indices.size());

        {
            ZoneScopedN("static_model.load_mesh.meshopt_generateVertexRemap");

            vertex_count = meshopt_generateVertexRemap(remap.data(),
                                                       indices.data(),
                                                       indices.size(),
                                                       raw_vertices.data(),
                                                       raw_vertices.size(),
                                                       sizeof(static_model::static_model_vertex));
        }

        std::vector<static_model::static_model_vertex> vertices(vertex_count);

        {
            ZoneScopedN("static_model.load_mesh.meshopt_remap[Vertex/Index]Buffer");

            meshopt_remapVertexBuffer(vertices.data(),
                                      raw_vertices.data(),
                                      raw_vertices.size(),
                                      sizeof(static_model::static_model_vertex),
                                      remap.data());
            meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
        }

        {
            ZoneScopedN("static_model.load_mesh.meshopt_optimizeVertexCache");
            meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
        }

        {
            ZoneScopedN("static_model.load_mesh.meshopt_optimizeVertexFetch");
            meshopt_optimizeVertexFetch(vertices.data(),
                                        indices.data(),
                                        indices.size(),
                                        vertices.data(),
                                        vertices.size(),
                                        sizeof(static_model::static_model_vertex));
        }

        return {indices, vertices};
#endif
    }

    // FIXME: use staging buffers
    void upload_to_scene(const static_model::mesh_data& mesh, const vk_renderer& renderer,
                         vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

#if 0
#else
        upload_data(
            renderer, geometry_pool.index.buffer, geometry_pool.index.offset, mesh.indices.data(), mesh.indices.size());
        upload_data(renderer,
                    geometry_pool.vertex.buffer,
                    geometry_pool.vertex.offset,
                    mesh.vertices.data(),
                    mesh.vertices.size());
#endif
    }
}

result<static_model> static_model::load_model(const bytes& data, render::vk_renderer& renderer,
                                              render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;
#if 1
    thread_local Assimp::Importer importer;
    thread_local std::vector<mesh_data> meshes;
    thread_local std::stack<aiNode*> process_nodes;
#else
    Assimp::Importer importer;
    std::vector<mesh_data> meshes;
    std::stack<aiNode*> process_nodes;
#endif
    const aiScene* scene = nullptr;
    {
        ZoneScopedN("static_model::load_model.importer.ReadFileFromMemory");

        constexpr auto flags =
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace;
        scene = importer.ReadFileFromMemory(data.data(), data.size(), flags);

        if ((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) || (scene->mRootNode == nullptr))
        {
            return importer.GetErrorString();
        }
    }

    {
        ZoneScopedN("static_model::load_model: convert data");

        meshes.clear();
        process_nodes.push(scene->mRootNode);

        while (!process_nodes.empty())
        {
            const auto* node = process_nodes.top();
            process_nodes.pop();

            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                // the node object only contains indices to index the actual objects in the scene.
                // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(load_mesh(mesh));
            }

            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                process_nodes.push(node->mChildren[i]);
            }
        }
    }

    offsets offsets {
        .vertex_offset = geometry_pool.vertex.offset,
        .index_offset  = geometry_pool.index.offset,
    };

    {
        ZoneScopedN("static_model::load_model: upload data");

        for (auto& mesh : meshes)
        {
            offsets.index_count += mesh.indices.size();
            offsets.vertex_count += mesh.vertices.size();

            upload_to_scene(mesh, renderer, geometry_pool);

            geometry_pool.index.offset += offsets.index_count;
            geometry_pool.vertex.offset += offsets.vertex_count;
        }
    }

    return static_model {offsets};
}

void static_model::draw(VkCommandBuffer buffer) const
{
    ZoneScoped;
    vkCmdDrawIndexed(buffer, m_offsets.index_count, 1, m_offsets.index_offset, 0, 0);
}

u32 static_model::indices_count() const
{
    return m_offsets.index_count;
}

static_model::static_model(const offsets& offsets)
    : m_offsets(offsets)
{
}
