#include <editor/hierarchy.hpp>
#include <imgui.h>
#include <scene/components.hpp>
#include <scene/scene.hpp>
#include <scene/scene_hierarchy.hpp>

namespace
{
    const char* get_entity_label(const scene& scene, entt::entity e)
    {
#if !defined(NDEBUG)
        if (const auto* id = scene.try_get_component<id_component>(e))
        {
            if (!id->name.empty())
            {
                return id->name.c_str();
            }
        }
#endif

        return "[unnamed]";
    }
}

void editor::hierarchy_window_context::draw(scene& scene)
{
#if !defined(NDEBUG)
    if (ImGui::Begin("Hierarchy"))
    {
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        {
            m_selected_node = scene_hierarchy::kInvalidIndex;
        }

        for (const u32 root : scene.hierarchy.roots)
        {
            draw_node(scene, root);
        }
    }

    ImGui::End();
#endif
}

void editor::hierarchy_window_context::draw_node(scene& scene, u32 node_idx)
{
#if !defined(NDEBUG)
    const auto& node   = scene.hierarchy.nodes[node_idx];
    const bool is_leaf = node.children.empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (is_leaf)
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (m_selected_node == node_idx)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<uintptr_t>(node_idx)), flags, "%s", get_entity_label(scene, node.e));

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        m_selected_node = node_idx;
    }

    if (open && !is_leaf)
    {
        for (u32 child : node.children)
        {
            draw_node(scene, child);
        }

        ImGui::TreePop();
    }
#endif
}
