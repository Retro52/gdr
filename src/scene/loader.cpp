#include <assert2.hpp>
#include <cgltf.h>
#include <ddspp.h>
#include <janitor.hpp>
#include <job/schedule_async.hpp>
#include <job/wait_group.hpp>
#include <render/platform/vk/vk_utils.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/loader.hpp>
#include <scene/mesh_load.hpp>
#include <scene/scene.hpp>

#include <glm/gtc/type_ptr.inl>

#ifdef Success
#undef Success
#endif

#define CHECK(EXPR)                   \
    if (EXPR != cgltf_result_success) \
    {                                 \
        assert2m(false, #EXPR);       \
        return {};                    \
    }

namespace
{
    struct parsed_texture
    {
        bytes data;
        ddspp::Descriptor descriptor;
    };

    u16 pack_oct(const vec3& data)
    {
        const vec2 proj = vec2(data) * (1.0F / (abs(data.x) + abs(data.y) + abs(data.z)));
        const vec2 sign = vec2((data.x >= 0.0F) ? 1.0F : -1.0F, (data.y >= 0.0F) ? 1.0F : -1.0F);
        const vec2 r    = (data.z <= 0.0) ? ((1.0F - abs(vec2(proj.y, proj.x))) * sign) : proj;

        return (meshopt_quantizeSnorm(r.x, 8) + 127) | ((meshopt_quantizeSnorm(r.y, 8) + 127) << 8);
    }
}

static vec4 compute_bounding_sphere(const cpp::heap_array<mesh::raw_vertex>& mesh)
{
    ZoneScoped;
    vec3 center(0.0F);
    for (const auto& v : mesh)
    {
        center += v.position;
    }

    f32 radius = 0.0F;
    center /= mesh.size();

    for (const auto& v : mesh)
    {
        radius = glm::max(radius, glm::distance(center, v.position));
    }

    return {center, radius};
}

static loader::material build_material(const cgltf_data* data, const cgltf_material& material,
                                       const u32 base_texture = 1)
{
    loader::material mat {};

    auto tex_idx = [&](const cgltf_texture* texture) -> u32
    {
        if (!texture)
        {
            return 0;
        }

        return base_texture + static_cast<u32>(cgltf_texture_index(data, texture));
    };

    mat.normal_idx = tex_idx(material.normal_texture.texture);
    if (material.has_pbr_metallic_roughness)
    {
        mat.albedo_idx        = tex_idx(material.pbr_metallic_roughness.base_color_texture.texture);
        mat.met_roughness_idx = tex_idx(material.pbr_metallic_roughness.metallic_roughness_texture.texture);

        mat.diffuse_factor = vec4(material.pbr_metallic_roughness.base_color_factor[0],
                                  material.pbr_metallic_roughness.base_color_factor[1],
                                  material.pbr_metallic_roughness.base_color_factor[2],
                                  material.pbr_metallic_roughness.base_color_factor[3]);

        mat.met_roughness_factor = vec4(1.0F,
                                        material.pbr_metallic_roughness.roughness_factor,
                                        material.pbr_metallic_roughness.metallic_factor,
                                        1.0F);
    }
    else if (material.has_pbr_specular_glossiness)
    {
        mat.met_roughness_idx = tex_idx(material.pbr_specular_glossiness.specular_glossiness_texture.texture);
        mat.albedo_idx        = tex_idx(material.pbr_specular_glossiness.diffuse_texture.texture);
        mat.diffuse_factor    = vec4(material.pbr_specular_glossiness.diffuse_factor[0],
                                  material.pbr_specular_glossiness.diffuse_factor[1],
                                  material.pbr_specular_glossiness.diffuse_factor[2],
                                  material.pbr_specular_glossiness.diffuse_factor[3]);

        const f32 max_specular = glm::max(material.pbr_specular_glossiness.specular_factor[0],
                                          material.pbr_specular_glossiness.specular_factor[1],
                                          material.pbr_specular_glossiness.specular_factor[2]);

        mat.material_flags       = 1 << shader_constants::kMatGlossBit;
        mat.met_roughness_factor = vec4(1.0F, material.pbr_specular_glossiness.glossiness_factor, max_specular, 1.0F);
    }

    if (material.has_transmission && vec3(mat.diffuse_factor) == vec3(1.0F))
    {
        mat.diffuse_factor.a = 0.0F;
    }

    return mat;
}

static cpp::heap_array<loader::prim_info> collect_primitives(const cgltf_data* data,
                                                             cpp::heap_array<loader::mesh_info>& descriptors,
                                                             cpp::heap_array<cpp::stack_string>* debug_names)
{
    ZoneScoped;
    assert2(data);

    cpp::heap_array<loader::prim_info> result;
    result.reserve(data->meshes_count * 3);

    for (u32 i = 0; i < data->meshes_count; ++i)
    {
        const cgltf_mesh& mesh    = data->meshes[i];
        const u32 first_primitive = result.size();

        for (u64 prim = 0; prim < mesh.primitives_count; ++prim)
        {
            const auto& primitive = mesh.primitives[prim];
            if (primitive.type != cgltf_primitive_type_triangles || !primitive.indices || !primitive.attributes)
            {
                continue;
            }

            if (debug_names)
            {
                debug_names->emplace_back(cpp::stack_string::make_formatted(
                    "build_mesh %s#%d", mesh.name ? mesh.name : "[nameless]", static_cast<int>(prim)));
            }

            result.emplace_back(first_primitive + prim, &primitive);
        }
        descriptors[i] = {.offset = first_primitive, .prim_count = static_cast<u32>(result.size() - first_primitive)};
    }

    std::sort(result.begin(),
              result.end(),
              [](const loader::prim_info& lhs, const loader::prim_info& rhs)
              {
                  const auto* left  = lhs.ptr;
                  const auto* right = rhs.ptr;

                  return left && right && left->indices && right->indices
                      && left->indices->count > right->indices->count;
              });

    return result;
}

static result<parsed_texture> read_texture(const fs::path& path)
{
    ZoneScoped;
    const auto r_data = fs::read_file(path);
    if (!r_data)
    {
        return error("failed to open file");
    }

    ddspp::Descriptor desc {};
    ddspp::Result decode_r = ddspp::decode_header(r_data->get<u8>(), desc);

    if (decode_r != ddspp::Result::Success)
    {
        return error("failed to read texture as DDS");
    }

    return parsed_texture {.data = *r_data, .descriptor = desc};
}

static result<render::vk_image> upload_texture(const parsed_texture& texture, const render::vk_renderer& renderer,
                                               const render::vk_buffer_transfer& scratch)
{
    auto& data = texture.data;
    auto& desc = texture.descriptor;

    const VkImageCreateInfo image_create_info = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext       = nullptr,
        .imageType   = desc.type == ddspp::Texture2D ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
        .format      = render::vk_format_from_dxgi(desc.format),
        .extent      = {desc.width, desc.height, desc.depth},
        .mipLevels   = desc.numMips,
        .arrayLayers = desc.arraySize,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    };

    const auto image_r = render::create_image(
        renderer.get_context().device, image_create_info, VK_IMAGE_ASPECT_COLOR_BIT, renderer.get_context().allocator);

    if (!image_r)
    {
        return error(image_r.message);
    }

    const u8* data_bytes = data.get<u8>() + desc.headerSize;
    render::upload_image(scratch,
                         *image_r,
                         data_bytes,
                         data.length<u8>() - desc.headerSize,
                         desc.width,
                         desc.height,
                         desc.numMips,
                         desc.blockHeight,
                         desc.bitsPerPixelOrBlock);
    return *image_r;
}

u32 loader::get_max_lod_tris(const loader::primitive& prim)
{
    u32 res = 0;
    for (u32 i = 0; i < prim.lod_count; ++i)
    {
        res = cpp::max(prim.lod_array[i].indices_count / 3, res);
    }

    return res;
}

u32 loader::get_max_lod_meshlets(const loader::primitive& prim)
{
    u32 res = 0;
    for (u32 i = 0; i < prim.lod_count; ++i)
    {
        res = cpp::max(prim.lod_array[i].meshlets_count, res);
    }

    return res;
}

result<render::vk_image> loader::load_texture(const fs::path& path, const render::vk_renderer& renderer,
                                              const render::vk_buffer_transfer& scratch)
{
    ZoneScoped;
    if (const auto read_r = read_texture(path))
    {
        return upload_texture(*read_r, renderer, scratch);
    }

    return error("failed to read texture");
}

loader::stats loader::load_scene(const fs::path& path, scene& scene, const render::vk_renderer& renderer,
                                 render::vk_scene_geometry_pool& geometry_pool,
                                 cpp::heap_array<render::vk_image>& textures)
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

    loader_context ctx;

#if TRACY_ENABLE
    cpp::heap_array<cpp::stack_string> debug_names;
#endif

    cpp::heap_array<mesh_info> meshes(data->meshes_count);

    const auto to_load = collect_primitives(data,
                                            meshes,
#if TRACY_ENABLE
                                            &debug_names
#else
                                            nullptr
#endif
    );

    const u64 prim_count = to_load.size();
    cpp::heap_array<mesh::raw_mesh> raw_meshes(prim_count);

    job::wait_group wg(prim_count);
    job::wait_group textures_wg(0);

    {
        ZoneScopedN("loader::load_scene::schedule_load_meshes");
        for (u32 i = 0; i < prim_count; ++i)
        {
            job::schedule_async(
                [&, i]
                {
#if TRACY_ENABLE
                    TracyMessage(debug_names[i].c_str(), debug_names[i].length());
#endif
                    raw_meshes[to_load[i].id] = mesh::build_mesh(*to_load[i].ptr);
                },
                wg);
        }
    }

    textures.resize(data->textures_count);
    cpp::heap_array<parsed_texture> textures_data(data->textures_count);

    {
        ZoneScopedN("loader::load_scene::schedule_load_textures");

        for (u64 i = 0; i < data->textures_count; ++i)
        {
            const auto texture = data->textures[i];
            if (!texture.image->uri)
            {
                continue;
            }

            const auto tex_path  = fs::path(texture.image->uri);
            const auto ext_swap  = tex_path.stem().append(".dds");
            const auto full_path = path.parent() / tex_path.parent() / ext_swap;

            textures_wg.add(1);
            job::schedule_async(
                [&, i, full_path]()
                {
#if TRACY_ENABLE
                    const auto debug_name =
                        cpp::big_stack_string::make_formatted("read texture: %s", full_path.c_str());
                    TracyMessage(debug_name.c_str(), debug_name.length());
#endif

                    if (auto res = read_texture(full_path))
                    {
                        textures_data[i] = std::move(*res);
                    }
                },
                textures_wg);
        }
    }

    textures_wg.wait_till_done();
    for (u32 i = 0; i < textures_data.size(); ++i)
    {
        if (auto image_r = upload_texture(textures_data[i], renderer, geometry_pool.transfer))
        {
            textures[i] = *image_r;
        }
    }

    ctx.materials.resize(data->materials_count + 1);
    ctx.materials[0].diffuse_factor = vec4(1.0F, 0.0F, 0.71F, 1.0F);  // -> pinkish <-

    for (u32 i = 0; i < data->materials_count; ++i)
    {
        ctx.materials[i + 1] = build_material(data, data->materials[i]);
    }

    wg.wait_till_done();
    cpp::heap_array<prim_layout> layouts(prim_count);

    u64 total_vertices = 0;
    u64 total_indices  = 0;
    u64 total_meshlets = 0;
    u64 total_payload  = 0;
    for (u32 p = 0; p < prim_count; ++p)
    {
        auto& layout         = layouts[p];
        layout.prim_index    = p + geometry_pool.primitives.offset / sizeof(loader::primitive);
        layout.vertex_offset = total_vertices + (geometry_pool.vertex.offset / sizeof(loader::vertex));

        total_vertices += raw_meshes[p].raw_vertices.size();

        for (u32 i = 0; i < raw_meshes[p].lod_count; ++i)
        {
            const auto& lod     = raw_meshes[p].lod_array[i];
            layout.lod_array[i] = {total_indices + (geometry_pool.index.offset / sizeof(u32)),
                                   total_meshlets + (geometry_pool.meshlets.offset / sizeof(loader::meshlet)),
                                   total_payload + geometry_pool.meshlets_payload.offset};

            total_indices += lod.raw_indices.size();
            total_meshlets += lod.raw_meshlets.size();
            total_payload += lod.raw_meshlets_payload.size();
        }
    }

    ctx.primitives.resize(prim_count);
    ctx.vertices.resize(total_vertices);
    ctx.indices.resize(total_indices);
    ctx.meshlets.resize(total_meshlets);
    ctx.meshlets_data.resize(total_payload);

    {
        job::wait_group wg1(prim_count);
        for (u32 i = 0; i < prim_count; ++i)
        {
            job::schedule_async(
                [&, i]
                {
                    encode_raw_mesh(ctx, raw_meshes[i], layouts[i]);
                },
                wg1);
        }
        wg1.wait_till_done();
    }

    upload_data(geometry_pool.transfer, geometry_pool.index, ctx.indices.data(), ctx.indices.size());
    upload_data(geometry_pool.transfer, geometry_pool.primitives, ctx.primitives.data(), ctx.primitives.size());
    upload_data(geometry_pool.transfer, geometry_pool.vertex, ctx.vertices.data(), ctx.vertices.size());
    upload_data(geometry_pool.transfer, geometry_pool.meshlets, ctx.meshlets.data(), ctx.meshlets.size());
    upload_data(geometry_pool.transfer, geometry_pool.materials, ctx.materials.data(), ctx.materials.size());
    upload_data(
        geometry_pool.transfer, geometry_pool.meshlets_payload, ctx.meshlets_data.data(), ctx.meshlets_data.size());

#if !defined(NDEBUG)
    auto& hierarchy = scene.hierarchy;
    hierarchy.nodes.resize(data->nodes_count);
#endif

    // TODO: refactor (moving objects?)
    // Also need to be careful with not writing anything to transfer in the meantime
    auto* instances = static_cast<loader::instance*>(geometry_pool.transfer.mapped);

    u32 triangles_max     = 0;
    u32 instance_count    = 0;
    u32 visibility_offset = 0;

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
            auto& desc = meshes[cgltf_mesh_index(data, node->mesh)];

            for (u32 j = 0; j < desc.prim_count; ++j)
            {
                instances[instance_count].pos_and_scale     = {transform_comp.position, transform_comp.uniform_scale};
                instances[instance_count].rotation_quat     = transform_comp.rotation;
                instances[instance_count].visibility_offset = visibility_offset;
                instances[instance_count].mesh_data_index   = desc.offset + j;
                instances[instance_count].material_index =
                    node->mesh->primitives[j].material
                        ? (cgltf_material_index(data, node->mesh->primitives[j].material) + 1)
                        : 0;

                ++instance_count;
                triangles_max += get_max_lod_tris(ctx.primitives[desc.offset + j]);
                visibility_offset += get_max_lod_meshlets(ctx.primitives[desc.offset + j]);
            }
        }

        if (node->camera)
        {
            auto& component = entity.emplace_component<camera_component>();

            component.horizontal_fov = node->camera->data.perspective.yfov;
            component.near_plane     = node->camera->data.perspective.znear;
            component.aspect_ratio   = node->camera->data.perspective.aspect_ratio;
        }

        if (node->light && node->light->type == cgltf_light_type_directional)
        {
            auto& component     = entity.emplace_component<directional_light_component>();
            component.rgb_color = {node->light->color[0], node->light->color[1], node->light->color[2]};
        }

#if !defined(NDEBUG)
        auto& scene_node = hierarchy.nodes[cgltf_node_index(data, node)];
        scene_node.e     = entity.get_native();
        scene_node.children.reserve(node->children_count);
        for (size_t c = 0; c < node->children_count; ++c)
        {
            scene_node.children.push_back(static_cast<u32>(cgltf_node_index(data, node->children[c])));
        }

        if (node->parent)
        {
            scene_node.parent = cgltf_node_index(data, node->parent);
        }
        else
        {
            hierarchy.roots.emplace_back(cgltf_node_index(data, node));
        }
#endif
    }

    render::submit_transfer(geometry_pool.transfer,
                            geometry_pool.instances.buffer,
                            VkBufferCopy {.size = instance_count * sizeof(loader::instance)});
    geometry_pool.instances.offset += instance_count * sizeof(loader::instance);
    // TODO

    return {
        .meshes     = meshes.size(),
        .meshlets   = visibility_offset,
        .triangles  = triangles_max,
        .primitives = instance_count,
    };
}

result<loader::meshes_context> loader::load_meshes(const fs::path& path)
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

    loader::meshes_context ctx;
    ctx.meshes.resize(data->meshes_count);

#if TRACY_ENABLE
    cpp::heap_array<cpp::stack_string> debug_names;
#endif

    const auto prims_to_load = collect_primitives(data,
                                                  ctx.meshes,
#if TRACY_ENABLE
                                                  &debug_names
#else
                                                  nullptr
#endif

    );

    ctx.primitives.resize(prims_to_load.size());

    constexpr u32 kMinForAsync = 16;  // * magic number how cool *
    if (prims_to_load.size() > kMinForAsync)
    {
        const job::wait_group wg(prims_to_load.size());

        {
            ZoneScopedN("loader::load_scene::schedule_load_meshes");
            for (u32 i = 0; i < prims_to_load.size(); ++i)
            {
                job::schedule_async(
                    [&, i]
                    {
#if TRACY_ENABLE
                        TracyMessage(debug_names[i].c_str(), debug_names[i].length());
#endif
                        ctx.primitives[prims_to_load[i].id] = mesh::build_mesh(*prims_to_load[i].ptr);
                    },
                    wg);
            }
        }

        wg.wait_till_done();
    }
    else
    {
        for (u32 i = 0; i < prims_to_load.size(); ++i)
        {
            TracyMessage(debug_names[i].c_str(), debug_names[i].length());
            ctx.primitives[prims_to_load[i].id] = mesh::build_mesh(*prims_to_load[i].ptr);
        }
    }

    return ctx;
}

void loader::encode_raw_mesh(loader_context& ctx, const mesh::raw_mesh& primitive, const prim_layout& layout)
{
    ZoneScoped;

    // Create new primitive descriptor
    auto& prim_desc            = ctx.primitives[layout.prim_index];
    const vec4 bounding_sphere = compute_bounding_sphere(primitive.raw_vertices);

    prim_desc.base_vertex = layout.vertex_offset;
    prim_desc.lod_count   = primitive.lod_count;
    prim_desc.center[0]   = bounding_sphere.x;
    prim_desc.center[1]   = bounding_sphere.y;
    prim_desc.center[2]   = bounding_sphere.z;
    prim_desc.radius      = bounding_sphere.w;

    // Encode vertices
    for (u32 i = 0; i < primitive.raw_vertices.size(); ++i)
    {
        const auto& vertex = primitive.raw_vertices[i];
        auto& encoded_vtx  = ctx.vertices[layout.vertex_offset + i];

        encoded_vtx.px = vertex.position.x;
        encoded_vtx.py = vertex.position.y;
        encoded_vtx.pz = vertex.position.z;

        encoded_vtx.packed_normal |= (meshopt_quantizeSnorm(vertex.normal.x, 10) + 511) << 0;
        encoded_vtx.packed_normal |= (meshopt_quantizeSnorm(vertex.normal.y, 10) + 511) << 10;
        encoded_vtx.packed_normal |= (meshopt_quantizeSnorm(vertex.normal.z, 10) + 511) << 20;
        encoded_vtx.packed_normal |= (vertex.tangent.w < 0 ? 1 : 0) << 30;

        encoded_vtx.packed_tangent = pack_oct(vec3(vertex.tangent));

        encoded_vtx.ux = vertex.uv.x;
        encoded_vtx.uy = vertex.uv.y;
    }

    // Copy LOD array
    for (u32 i = 0; i < primitive.lod_count; ++i)
    {
        const auto& lod_level   = primitive.lod_array[i];
        const auto& lod_layout  = layout.lod_array[i];
        auto& encoded_lod_level = prim_desc.lod_array[i];

        encoded_lod_level.base_index    = lod_layout.index_offset;
        encoded_lod_level.indices_count = lod_level.raw_indices.size();

        encoded_lod_level.base_meshlet   = lod_layout.meshlet_offset;
        encoded_lod_level.meshlets_count = lod_level.raw_meshlets.size();

        encoded_lod_level.error = lod_level.error;

        const u64 meshlet_base_offset = lod_layout.meshlet_data_offset;

        // simple copy
        std::memcpy(ctx.indices.data() + lod_layout.index_offset,
                    lod_level.raw_indices.data(),
                    lod_level.raw_indices.size() * sizeof(u32));
        std::memcpy(ctx.meshlets_data.data() + lod_layout.meshlet_data_offset,
                    lod_level.raw_meshlets_payload.data(),
                    lod_level.raw_meshlets_payload.size());

        // encode meshlets
        for (u32 j = 0; j < lod_level.raw_meshlets.size(); ++j)
        {
            const auto& meshlet = lod_level.raw_meshlets[j];
            auto& encoded_mst   = ctx.meshlets[lod_layout.meshlet_offset + j];

            encoded_mst.cone_axis[0] = meshlet.cone_axis.x;
            encoded_mst.cone_axis[1] = meshlet.cone_axis.y;
            encoded_mst.cone_axis[2] = meshlet.cone_axis.z;

            encoded_mst.sphere_center[0] = meshlet.sphere_center.x;
            encoded_mst.sphere_center[1] = meshlet.sphere_center.y;
            encoded_mst.sphere_center[2] = meshlet.sphere_center.z;

            encoded_mst.cone_cutoff     = meshlet.cone_cutoff;
            encoded_mst.sphere_radius   = meshlet.sphere_radius;
            encoded_mst.triangles_count = meshlet.triangles_count;
            encoded_mst.vertices_count  = meshlet.vertices_count;
            encoded_mst.data_offset     = meshlet_base_offset + meshlet.data_prefix_sum;
        }
    }
}
