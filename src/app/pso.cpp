#include <app/pso.hpp>
#include <fs/fs.hpp>
#include <tracy/Tracy.hpp>

void app::pso_data::load(const render::vk_renderer& renderer, const render::vk_descriptor_set& textures_set)
{
    ZoneScoped;
    auto data = fs::read_file("../shaders/pipelines.cfg");
    if (!data)
    {
        assert2m(false, data.message);
        return;
    }

    const auto* c = data.value.get<char>();

    cpp::big_stack_string buffer;

    u32 key = 0;
    cpp::heap_array<cpp::stack_string> shaders;
    std::unordered_map<u32, render::vk_shader> cache;

    constexpr fs::path kShadersBinDir = "../shaders/bin";

    auto process = [&]()
    {
        cpp::heap_array<render::vk_shader> compiled;
        for (const auto& shader : shaders)
        {
            const auto shader_id = cpp::crc::crc32(shader.c_str(), shader.length());
            const auto it        = cache.find(shader_id);
            if (it == cache.end())
            {
                const auto compiled_shader = render::vk_shader::load(renderer, kShadersBinDir / shader);
                assert2m(compiled_shader, compiled_shader.message);
                if (compiled_shader)
                {
                    cache.emplace(shader_id, *compiled_shader);
                    compiled.emplace_back(*compiled_shader);
                }
            }
            else
            {
                compiled.emplace_back(it->second);
            }
        }

        assert2m(!compiled.empty() && compiled.size() == shaders.size(), "some shaders failed to compile?");
        if (compiled.size() == shaders.size())
        {
            // stupid...
            auto pso = (shaders.size() == 1) ? render::vk_pipeline::create_compute(renderer, compiled[0])
                                             : render::vk_pipeline::create_graphics(
                                                   renderer, compiled.data(), compiled.size(), &textures_set, 1);

            assert2m(pso && key, pso.message);
            if (pso)
            {
                m_pipelines[key] = *pso;
            }
        }

        key = 0;
        shaders.clear();
    };

    while (c && *c)
    {
        switch (*c)
        {
        case '\r' :
            break;
        case '\n' :
            if (key && !buffer.empty())
            {
                shaders.emplace_back(buffer);
                buffer.clear();
            }

            if (key && !shaders.empty())
            {
                process();
            }
            break;
        case ';' :
            key = cpp::crc::crc32(buffer.c_str(), buffer.length());
            buffer.clear();
            break;
        case ' ' :
            if (key && !buffer.empty())
            {
                shaders.emplace_back(buffer);
                buffer.clear();
            }
            break;
        default :
            buffer += *c;
            break;
        }

        ++c;
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
