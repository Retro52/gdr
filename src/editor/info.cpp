#include <camera_controller.hpp>
#include <codegen/camera_controller.hpp>
#include <codegen/imgui/gpu_profile_data.hpp>
#include <codegen/scene/components.hpp>
#include <editor/info.hpp>
#include <imgui.h>
#include <imgui/imex.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>

namespace
{
    f64 bytes_to_mb(u64 bytes)
    {
        return static_cast<f64>(bytes) / (1024.0 * 1024);
    }

    cpp::stack_string format_big_number(u64 number)
    {
        ZoneScoped;

        if (number < 1'000)
        {
            return cpp::stack_string::make_formatted("%d", number);
        }

        const char* magnitudes_per_thousand[] = {"K", "M", "B", "T"};

        auto magnitude     = static_cast<i32>(std::log10(number) / 3);
        const f64 fraction = static_cast<f64>(number) / glm::pow(1000, magnitude);
        return cpp::stack_string::make_formatted("%.3lf%s", fraction, magnitudes_per_thousand[magnitude - 1]);
    }

    void draw_shared_buffer_stats(const char* label, const render::vk_shared_buffer& buffer)
    {
        ZoneScoped;

        const auto fraction = static_cast<f32>(buffer.offset) / static_cast<f32>(buffer.size);
        const auto str      = cpp::stack_string::make_formatted("%s buffer: %.4lf/%.4lf MB used (%.2lf%%)",
                                                           label,
                                                           bytes_to_mb(buffer.offset),
                                                           bytes_to_mb(buffer.size),
                                                           fraction * 100.0F);
        ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F), str.c_str());
    }

    void draw_scene_geometry_pool(const render::vk_scene_geometry_pool& buffer)
    {
        ZoneScoped;
        const u64 acc_size = buffer.primitives.size + buffer.meshlets.size + buffer.index.size
                           + buffer.meshlets_payload.size + buffer.instances.size + buffer.vertex.size
                           + buffer.materials.size;

        const u64 acc_offset = buffer.primitives.offset + buffer.meshlets.offset + buffer.index.offset
                             + buffer.meshlets_payload.offset + buffer.instances.offset + buffer.vertex.offset
                             + buffer.materials.offset;

        const auto fraction = static_cast<f32>(acc_offset) / static_cast<f32>(acc_size);

        ImGui::Separator();
        const auto str = cpp::stack_string::make_formatted("All buffers space used: %.4lf/%.4lf MB used (%.2lf%%)",
                                                           bytes_to_mb(acc_offset),
                                                           bytes_to_mb(acc_size),
                                                           fraction * 100.0F);

        ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F), str.c_str());
    }
}

void editor::info_widget_context::draw() const
{
    ImGui::SeparatorText("camera controller");
    codegen::draw(m_camera);
    codegen::draw(m_camera.active_camera().get_component<transform_component>());

    ImGui::SeparatorText("gpu timings");
    codegen::draw(m_gpu_profile);

    const auto str = cpp::stack_string::make_formatted("Total triangles rendered: %.3F%%",
                                                       static_cast<f32>(m_gpu_profile.tris_from_max) * 100.0F);
    ImGui::ProgressBar(static_cast<f32>(m_gpu_profile.tris_from_max), ImVec2(0.0F, 0.0F), str.c_str());
    ImGui::Text("Tris Max: %s", format_big_number(m_gpu_profile.tris_in_scene_max).c_str());
    ImGui::Text("Tris Drawn: %s", format_big_number(m_gpu_profile.tris_in_scene_total).c_str());

    if (ImGui::CollapsingHeader("Geometry pool stats"))
    {
        draw_shared_buffer_stats("Vertices", m_geometry_pool.vertex);
        draw_shared_buffer_stats("Indices", m_geometry_pool.index);
        draw_shared_buffer_stats("Meshlets", m_geometry_pool.meshlets);
        draw_shared_buffer_stats("Instances", m_geometry_pool.instances);
        draw_shared_buffer_stats("Primitives", m_geometry_pool.primitives);
        draw_shared_buffer_stats("Materials data", m_geometry_pool.materials);
        draw_shared_buffer_stats("Meshlets payload", m_geometry_pool.meshlets_payload);
        draw_scene_geometry_pool(m_geometry_pool);
    }
}

void editor::info_widget_context::draw(const char* label, const app::pipeline_statistics_data& pipeline_stats) const
{
#if !NO_PERF_QUERY
    if (ImGui::CollapsingHeader(label))
    {
        ImGui::Text("input_assembly_vertices: %s", format_big_number(pipeline_stats.input_assembly_vertices).c_str());
        ImGui::Text("input_assembly_primitives: %s",
                    format_big_number(pipeline_stats.input_assembly_primitives).c_str());
        ImGui::Text("vertex_shader_invocations: %s",
                    format_big_number(pipeline_stats.vertex_shader_invocations).c_str());
        ImGui::Text("triangles_count: %s", format_big_number(pipeline_stats.triangles_count).c_str());
        ImGui::Text("fragment_shader_invocations: %s",
                    format_big_number(pipeline_stats.fragment_shader_invocations).c_str());
    }
#endif
}
