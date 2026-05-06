#pragma once

#include <events.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <window.hpp>

namespace app
{
    struct instance
    {
    public:
        explicit instance();

        int run(int argc, char* argv[]);

    private:
        window m_window;
        events_queue m_events_queue;
        render::vk_renderer m_renderer;
    };
}
