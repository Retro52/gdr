#include <app/pso.hpp>
#include <cpp/containers/heap_array.hpp>
#include <cpp/hash/hashed_string.hpp>
#include <fs/fs.hpp>
#include <log.hpp>
#include <nlohmann/json.hpp>
#include <reflection/enum.hpp>
#include <tracy/Tracy.hpp>

namespace
{
    constexpr fs::path kShadersBinDir = "../shaders/bin";

    u64 get_last_write_time()
    {
        u64 result = 0;
        for (auto it = std::filesystem::directory_iterator(kShadersBinDir.c_str());
             it != std::filesystem::directory_iterator();
             ++it)
        {
            result = cpp::max(result, static_cast<u64>(it->last_write_time().time_since_epoch().count()));
        }

        return result;
    }

    template<typename T, typename U = T>
    bool opt_get_type(const nlohmann::json& options, const char* key, U& dst)
    {
        if (options.contains(key))
        {
            dst = static_cast<T>(options[key]);
            return true;
        }

        return false;
    }

    template<typename T>
    bool opt_get(const nlohmann::json& options, const char* key, T& dst)
    {
        return opt_get_type<T>(options, key, dst);
    }

    bool opt_get_bit(const nlohmann::json& options, const char* key, bool default_value = false)
    {
        opt_get_type<bool>(options, key, default_value);
        return default_value;
    }

    template<typename T>
    bool opt_get_enum(const nlohmann::json& options, const char* key, T& dst)
    {
        if (options.contains(key))
        {
            dst = static_cast<T>(reflection::enum_from_string<T>(options[key].get<std::string>().c_str()));
            return true;
        }

        return false;
    }

    void parse_pipeline_options(rhi::pso_options& options, const nlohmann::json& json_options)
    {
        opt_get_enum<rhi::image_format>(json_options, "depth_format", options.depth_format);

        opt_get_enum(json_options, "topology", options.topology);
        options.depth_bias_enable  = opt_get_bit(json_options, "depth_bias", options.depth_bias_enable);
        options.depth_clamp_enable = opt_get_bit(json_options, "depth_clamp", options.depth_clamp_enable);

        if (json_options.contains("color_formats"))
        {
            options.color_attachments_count = 0;
            for (auto& item : json_options["color_formats"].items())
            {
                auto format =
                    reflection::enum_from_string<rhi::image_format>(item.value().get<std::string>().c_str());

                options.add_color_attachment(static_cast<rhi::image_format>(format));
            }
        }
    }
}

void app::pso_data::load(const rhi::impl& rhi, const rhi::context context,
                         const rhi::swapchain swapchain, rhi::bindless_set textures_set)
{
    ZoneScoped;
    auto data = fs::read_file("../shaders/pipelines.json");
    if (!data)
    {
        assert2m(false, data.message);
        return;
    }

    const auto sc_format = RHI_SAFE_CALL(rhi.query_swapchain_color_format, swapchain);
    assert2(sc_format);

    if (!sc_format)
    {
        return;
    }

    nlohmann::json info = nlohmann::json::parse(data->get<char>(), data->get<char>() + data->size());

    std::unordered_map<u32, rhi::shader> cache;
    cpp::heap_array<rhi::shader> compiled_shaders;

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
                if (it.value() == "mesh_ext"
                    && !*rhi.query_feature_support(context, rhi::feature_flag::mesh_shading))
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
                const auto compiled_shader = RHI_SAFE_CALL(rhi.create_shader, context, kShadersBinDir / shader);
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
            auto options = rhi::pso_options().add_color_attachment(*sc_format);
            if (pipeline_info.contains("options"))
            {
                parse_pipeline_options(options, pipeline_info["options"]);
            }

            auto pso = (shaders.size() == 1
                        && (*RHI_SAFE_CALL(rhi.query_shader_stage, compiled_shaders.front())
                            == rhi::shader_stage::compute))
                         ? RHI_SAFE_CALL(rhi.create_compute_pso, context, compiled_shaders[0], {&textures_set, 1})
                         : RHI_SAFE_CALL(rhi.create_graphics_pso,
                                         context,
                                         {compiled_shaders.data(), compiled_shaders.size()},
                                         {&textures_set, 1},
                                         options);

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
        rhi.destroy_shader(context, shader);
    }
}

void app::pso_data::destroy(const rhi::impl& rhi, const rhi::context context, const pso_id id)
{
    ZoneScoped;
    RHI_SAFE_CALL(rhi.destroy_pso, context, this->operator[](id));
}

void app::pso_data::shutdown(const rhi::impl& rhi, const rhi::context context)
{
    ZoneScoped;
    for (auto& [_, pso] : m_pipelines)
    {
        RHI_SAFE_CALL(rhi.destroy_pso, context, pso);
    }
}

app::pso_watcher::pso_watcher(pso_data& pipelines, rhi::impl& rhi, const rhi::context context,
                              const rhi::bindless_set textures_set)
    : m_pdata(pipelines)
    , m_context(context)
    , m_textures_set(textures_set)
    , m_rhi(rhi)
{
    m_terminate = false;
    m_worker    = std::thread(
        [this]
        {
            this->m_last_write_time = get_last_write_time();
            while (!m_terminate)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                if (m_terminate || this->m_last_write_time == get_last_write_time())
                {
                    continue;
                }

                // this->m_renderer.schedule_delete(
                //     [&](VkDevice /* device */, VmaAllocator /* allocator */)
                //     {
                //         this->m_pdata.shutdown(this->m_renderer);
                //         this->m_pdata.load(this->m_renderer, this->m_textures_set);
                //
                //         this->m_last_write_time = get_last_write_time();
                //     });
            }
        });
}

void app::pso_watcher::shutdown()
{
    m_terminate = true;
    m_worker.join();
}
