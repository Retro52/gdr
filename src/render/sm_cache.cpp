#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cpp/alg_constexpr.hpp>
#include <fs/fs.hpp>
#include <meshoptimizer.h>
#include <render/sm_cache.hpp>
#include <render/sm_serializer.hpp>
#include <render/static_model.hpp>

#include <filesystem>
#include <stack>

namespace
{
    constexpr fs::path kCacheDir = ".vertex_cache";

    fs::path get_cache_path(const fs::path& path)
    {
        const u64 last_edit  = std::filesystem::last_write_time(path.c_str()).time_since_epoch().count();
        const auto full_name = fs::path_string::make_formatted(".%s_%lu", path.basename().c_str(), last_edit);

        return fs::path::current_path() / kCacheDir / full_name;
    }

    bool has_cache(const fs::path& path)
    {
        return std::filesystem::exists(get_cache_path(path).c_str());
    }
}

using sm_vertex    = static_model::vertex;
using sm_mesh_data = render::mesh_data<sm_vertex>;

template<typename T>
render::mesh_data<T> load_mesh(const aiMesh* mesh) noexcept;

template<>
sm_mesh_data load_mesh<sm_vertex>(const aiMesh* mesh) noexcept
{
    ZoneScoped;
    assert(mesh->HasNormals());

    std::vector<sm_vertex> raw_vertices(mesh->mNumVertices);
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

    u64 vertex_count = 0;
    std::vector<u32> remap(indices.size());

    {
        ZoneScopedN("meshopt_generateVertexRemap");

        vertex_count = meshopt_generateVertexRemap(
            remap.data(), indices.data(), indices.size(), raw_vertices.data(), raw_vertices.size(), sizeof(sm_vertex));
    }

    std::vector<sm_vertex> vertices(vertex_count);

    {
        ZoneScopedN("meshopt_remap[Vertex/Index]Buffer");

        meshopt_remapVertexBuffer(
            vertices.data(), raw_vertices.data(), raw_vertices.size(), sizeof(sm_vertex), remap.data());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
    }

#if !SM_USE_MESHLETS
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
#endif

    return {.vertices = vertices, .indices = indices};
}

template<typename T>
bool load_from_cache(const fs::path& path, std::vector<T>& meshes)
{
    if (has_cache(path))
    {
        ZoneScopedN("load from cache");

        u32 mesh_count   = 0;
        auto model_cache = render::load_model_cache(get_cache_path(path), mesh_count);
        if (!model_cache || mesh_count == 0)
        {
            return false;
        }

        u64 data_pointer = 0;
        meshes.resize(mesh_count);
        for (u32 i = 0; i < mesh_count; i++)
        {
            auto& data = meshes[i];

            u64 indices_count   = 0;
            u64 vertices_count  = 0;
            u64 vertices_stride = 0;

            render::load_mesh_cache_stats(
                &(*model_cache)[data_pointer], indices_count, vertices_count, vertices_stride);

            assert(vertices_stride == sizeof(sm_vertex));

            data.indices.resize(indices_count);
            data.vertices.resize(vertices_count);

            render::load_mesh_cache_data(&(*model_cache)[data_pointer],
                                         data.indices.data(),
                                         data.vertices.data(),
                                         indices_count,
                                         vertices_count,
                                         vertices_stride);

            data_pointer += indices_count * sizeof(u32) + vertices_count * vertices_stride;
        }

        return true;
    }

    return false;
}

template<>
result<std::vector<sm_mesh_data>> render::parse_model<sm_vertex>(const fs::path& path)
{
    ZoneScoped;

    auto contents = fs::read_file(path);
    if (!contents)
    {
        return contents.message;
    }

    const auto& data = contents.value;

    const aiScene* scene = nullptr;
    std::vector<sm_mesh_data> meshes;

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

        if ((scene == nullptr) || ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) || (scene->mRootNode == nullptr))
        {
            return importer.GetErrorString();
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
                // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(load_mesh<sm_vertex>(mesh));
            }

            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                process_nodes.push(node->mChildren[i]);
            }
        }
    }

    return meshes;
}

template<>
bool render::load_model<sm_vertex>(const fs::path& path, std::vector<sm_mesh_data>& meshes)
{
    ZoneScoped;

    if (!std::filesystem::exists(path.c_str()))
    {
        return false;
    }

    if (load_from_cache(path, meshes))
    {
        return true;
    }

    auto parsed = parse_model<sm_vertex>(path);
    if (!parsed)
    {
        return false;
    }

    meshes = parsed.value;

    {
        ZoneScopedN("create cache entry");

        std::vector<bytes> cache;
        cache.reserve(meshes.size());

        for (auto& mesh_data : meshes)
        {
            cache.push_back(*render::serialize_mesh_cache(mesh_data.indices.data(),
                                                          mesh_data.indices.size(),
                                                          mesh_data.vertices.data(),
                                                          mesh_data.vertices.size(),
                                                          sizeof(sm_vertex)));
        }

        auto model_cache = render::serialize_model_cache(cache.data(), cache.size());
        if (!model_cache)
        {
            // we failed to cache, but still succeeded to load
            return true;
        }

        fs::write_file(get_cache_path(path), *model_cache);
    }

    return true;
}
