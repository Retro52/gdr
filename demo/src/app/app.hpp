#pragma once

#include <app/argv.hpp>
#include <events.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <window.hpp>

namespace app
{
    struct instance
    {
    public:
        explicit instance(int argc, char* argv[]);

        int run();

    private:
        app::argv_handler m_args;

        window m_window;
        events_queue m_events_queue;
        render::vk_renderer m_renderer;
    };
}
