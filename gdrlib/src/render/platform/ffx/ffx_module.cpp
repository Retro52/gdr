#include <render/platform/ffx/ffx_module.hpp>
#include <tracy/Tracy.hpp>

#include <windows.h>

#define FFX_LOAD_SYMBOL(MODULE, NAME) this->NAME = reinterpret_cast<decltype(this->NAME)>(GetProcAddress(MODULE, #NAME))

platform::ffx_module::~ffx_module()
{
    ZoneScoped;
    FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
}

void platform::ffx_module::lazy_load(const char* path)
{
    if (m_handle)
    {
        return;
    }

    if (const auto win_handle = LoadLibrary(path))
    {
        m_handle = reinterpret_cast<u64>(win_handle);
        FFX_LOAD_SYMBOL(win_handle, ffxQuery);
        FFX_LOAD_SYMBOL(win_handle, ffxDispatch);
        FFX_LOAD_SYMBOL(win_handle, ffxConfigure);
        FFX_LOAD_SYMBOL(win_handle, ffxCreateContext);
        FFX_LOAD_SYMBOL(win_handle, ffxDestroyContext);
    }
}
