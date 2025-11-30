#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <render/platform/vk/vk_error.hpp>
#include <render/static_model.hpp>

using namespace render;

namespace
{
    // TODO: staging buffer
    void upload_data(const vk_renderer& renderer, const vk_buffer& buffer, const void* data, const u64 size)
    {
        const auto allocator = renderer.get_context().allocator;

        void* mapped;
        vmaMapMemory(allocator, buffer.allocation, &mapped);
        memcpy(mapped, data, size);
        vmaUnmapMemory(allocator, buffer.allocation);
    }

    static_model::mesh_data load_mesh(const aiMesh* mesh) noexcept
    {
        assert(mesh->HasNormals());

        static_model::mesh_data data;
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            if (mesh->mTextureCoords[0]) [[likely]]
            {
                data.vertices.emplace_back() = {
                    .position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z},
                    .normal   = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z},
                    .uv       = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y},
                    .tangent  = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z},
                };
            }
            else
            {
                data.vertices.emplace_back() = {
                    .position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z},
                    .normal   = {mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z }
                };
            }
        }

        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding
        // vertex indices.
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                data.indices.push_back(face.mIndices[j]);
            }
        }

        return data;
    }

    static_model::mesh_buffers make_buffers_for_mesh(const vk_renderer& renderer, const static_model::mesh_data& mesh)
    {
        static_model::mesh_buffers buffers {
            .indices_count = mesh.indices.size(),
        };

        const VkBufferCreateInfo vertex_buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = mesh.vertices.size() * sizeof(static_model::static_model_vertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        };

        VK_ASSERT_ON_FAIL(render::create_buffer(vertex_buffer_create_info,
                                                renderer.get_context().allocator,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                &buffers.vertex));

        const VkBufferCreateInfo index_buffer_create_info {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = mesh.indices.size() * sizeof(static_model::static_model_index),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        };

        VK_ASSERT_ON_FAIL(render::create_buffer(index_buffer_create_info,
                                                renderer.get_context().allocator,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                &buffers.index));

        upload_data(renderer, buffers.index, mesh.indices.data(), index_buffer_create_info.size);
        upload_data(renderer, buffers.vertex, mesh.vertices.data(), vertex_buffer_create_info.size);

        return buffers;
    }
}

result<static_model> static_model::load_model(render::vk_renderer& renderer, const bytes& data)
{
    thread_local Assimp::Importer importer;
    thread_local std::vector<mesh_data> meshes;
    thread_local std::stack<aiNode*> process_nodes;

    constexpr auto flags =
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace;
    auto* scene = importer.ReadFileFromMemory(data.data(), data.size(), flags);

    if ((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) || (scene->mRootNode == nullptr))
    {
        return importer.GetErrorString();
    }

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

    std::vector<mesh_buffers> mesh_buffers(meshes.size());
    for (u32 i = 0; i < mesh_buffers.size(); i++)
    {
        mesh_buffers[i] = make_buffers_for_mesh(renderer, meshes[i]);
    }

    return static_model {mesh_buffers};
}

void static_model::draw(VkCommandBuffer buffer)
{
    const VkDeviceSize offset = 0;
    for (const auto& mesh : m_meshes)
    {
        vkCmdBindVertexBuffers(buffer, 0, 1, &mesh.vertex.buffer, &offset);
        vkCmdBindIndexBuffer(buffer, mesh.index.buffer, offset, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(buffer, mesh.indices_count, 1, 0, 0, 0);
    }
}

static_model::static_model(const std::vector<mesh_buffers>& meshes)
    : m_meshes(meshes)
{
}
