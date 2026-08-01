#include <app/pso.hpp>
#include <fs/fs.hpp>
#include <log.hpp>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

void app::pso_data::load(const render::vk_renderer& renderer, const render::vk_descriptor_set& textures_set)
{
    ZoneScoped;
    auto data = fs::read_file("../shaders/pipelines.json");
    if (!data)
    {
        assert2m(false, data.message);
        return;
    }

    nlohmann::json info = nlohmann::json::parse(data->get<char>(), data->get<char>() + data->size());

    constexpr fs::path kShadersBinDir = "../shaders/bin";
    std::unordered_map<u32, render::vk_shader> cache;
    cpp::heap_array<render::vk_shader> compiled_shaders;

    auto process = [&](const u32 key, const nlohmann::json& pipeline_info)
    {
        if (!pipeline_info.contains("shaders"))
        {
            return;
        }

        compiled_shaders.clear();
        const auto& shaders = pipeline_info["shaders"];

        if (pipeline_info.contains("capabilities"))
        {
            const auto& capabilities = pipeline_info["capabilities"];
            for (auto it = capabilities.begin(); it != capabilities.end(); ++it)
            {
                if (it.value() == "mesh_ext" && !renderer.is_feature_supported(render::feature_flag::eMeshShading))
                {
                    LOG_WARNING("pipeline is skipped because mesh shaders are unsupported on this platform");
                    return;
                }
            }
        }

        for (const auto& shader : shaders)
        {
            const auto id        = shader.get<std::string>();
            const auto shader_id = cpp::crc::crc32(id.c_str(), id.length());
            const auto it        = cache.find(shader_id);
            if (it == cache.end())
            {
                const auto compiled_shader = render::vk_shader::load(renderer, kShadersBinDir / shader);
                assert2m(compiled_shader, compiled_shader.message);
                if (compiled_shader)
                {
                    cache.emplace(shader_id, *compiled_shader);
                    compiled_shaders.emplace_back(*compiled_shader);
                }
                else
                {
                    LOG_WARNING("Failed to load shader {}", id);
                    return;
                }
            }
            else
            {
                compiled_shaders.emplace_back(it->second);
            }
        }

        assert2m(!compiled_shaders.empty() && compiled_shaders.size() == shaders.size(),
                 "some shaders failed to compile?");
        if (compiled_shaders.size() == shaders.size())
        {
            auto pso = (shaders.size() == 1 && (compiled_shaders.front().meta.stage & VK_SHADER_STAGE_COMPUTE_BIT))
                         ? render::vk_pipeline::create_compute(renderer, compiled_shaders[0], &textures_set, 1)
                         : render::vk_pipeline::create_graphics(
                               renderer,
                               compiled_shaders.data(),
                               compiled_shaders.size(),
                               &textures_set,
                               1,
                               pipeline_info.contains("options") ? pipeline_info["options"] : nlohmann::json());

            assert2m(pso && key, pso.message);
            if (pso)
            {
                m_pipelines[key] = *pso;
            }
        }
    };

    for (auto it = info.begin(); it != info.end(); ++it)
    {
        if (!it.value().contains("shaders"))
        {
            LOG_WARNING("pipeline '{}' has no shader modules", it.key());
            continue;
        }

        const auto key = cpp::crc::crc32(it.key().c_str(), it.key().length());
        process(key, *it);
    }

    for (auto& [_, shader] : cache)
    {
        render::destroy_shader(renderer.get_context().device, shader);
    }
}

void app::pso_data::destroy(const render::vk_renderer& renderer, const pso_id id)
{
    ZoneScoped;
    auto& pso         = this->operator[](id);
    const auto device = renderer.get_context().device;

    render::destroy_pipeline(device, pso);
}

void app::pso_data::shutdown(const render::vk_renderer& renderer)
{
    ZoneScoped;
    for (auto& [_, pso] : m_pipelines)
    {
        render::destroy_pipeline(renderer.get_context().device, pso);
    }
}
