#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <app/app.hpp>

int main(const int argc, char* argv[])
{
    srand(322);  // NOLINT(*-msc51-cpp)
    TracySetProgramName("gdr");

    app::instance application;
    return application.run(argc, argv);
}
