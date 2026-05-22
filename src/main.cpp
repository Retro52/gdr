#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <app/app.hpp>

int main(const int argc, char* argv[])
{
    srand(322);  // NOLINT(*-msc51-cpp)
    TracySetProgramName("gdr");

#if TRACY_ENABLE
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(500ms);
#endif

    app::instance application;
    return application.run(argc, argv);
}
