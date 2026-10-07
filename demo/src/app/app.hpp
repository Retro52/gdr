#pragma once

#include <app/argv.hpp>
#include <events.hpp>
#include <render/rhi.hpp>
#include <window.hpp>

namespace app
{
    struct instance
    {
    public:
        explicit instance(int argc, char* argv[]);

        int run();

    private:
        window m_window;
        app::argv_handler m_args;

        render::rhi::rhi m_rhi;
        events_queue m_events_queue;
    };
}
