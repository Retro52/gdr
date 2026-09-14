#include <SDL3/SDL_platform_defines.h>

#include <assert2.hpp>
#include <debug.hpp>

#if defined(SDL_PLATFORM_WINDOWS)
#include <Windows.h>

#include <debugapi.h>
#elif defined(SDL_PLATFORM_MACOS)
#include <assert.h>
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(SDL_PLATFORM_LINUX)
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void debug::break_into_debugger()
{
    // __asm__ instructions used from ImGui
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__clang__)
    __builtin_debugtrap();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile("int3;nop");
#elif defined(__GNUC__) && defined(__thumb__)
    __asm__ volatile(".inst 0xde01");
#elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
    __asm__ volatile(".inst 0xe7f001f0");
#else
    assert2m(false, "triggered an assertion because compiler is unknown");
#endif
}

bool debug::is_debugger_present()
{
#if defined(SDL_PLATFORM_WINDOWS)
    return ::IsDebuggerPresent();
#elif defined(SDL_PLATFORM_MACOS)
    struct kinfo_proc info;
    info.kp_proc.p_flag = 0;

    size_t size    = sizeof(info);
    int mib[4]     = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    const int junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

    assert2(junk == 0);
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#elif defined(SDL_PLATFORM_LINUX)
    // https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb
    char buf[4096];
    const int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
    {
        return false;
    }

    const ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);
    close(status_fd);

    if (num_read <= 0)
    {
        return false;
    }

    buf[num_read]             = '\0';
    constexpr char pid_str[]  = "TracerPid:";
    const auto tracer_pid_ptr = strstr(buf, pid_str);
    if (!tracer_pid_ptr)
    {
        return false;
    }

    for (const char* c_ptr = tracer_pid_ptr + sizeof(pid_str) - 1; c_ptr <= buf + num_read; ++c_ptr)
    {
        if (isspace(*c_ptr))
        {
            continue;
        }

        return isdigit(*c_ptr) != 0 && *c_ptr != '0';
    }

    return false;
#else
    return false;
#endif
}
