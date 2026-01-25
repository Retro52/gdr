#pragma once

#include <types.hpp>

#include <soft_assert/soft_assert.hpp>

#include <string>

#if defined(_WIN32)
// clang-format off
#include <Windows.h>
#include <CommCtrl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// clang-format on
#endif

#if defined(_WIN32)
struct handle_data
{
    unsigned long process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    handle_data& data        = *(handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !is_main_window(handle))
    {
        return TRUE;
    }
    data.window_handle = handle;
    return FALSE;
}

HWND find_main_window(unsigned long process_id)
{
    handle_data data;
    data.process_id    = process_id;
    data.window_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}
#endif

void sa_show_assert_popup(const char* message)
{
#if defined(_WIN32)
    int msg_wchar_len = MultiByteToWideChar(CP_UTF8, 0, message, static_cast<int>(strlen(message)), nullptr, 0);
    std::vector<wchar_t> msg_wchar(msg_wchar_len + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, message, static_cast<int>(strlen(message)), msg_wchar.data(), msg_wchar_len);

    TASKDIALOGCONFIG config   = {sizeof(config)};
    config.hwndParent         = find_main_window(GetCurrentProcessId());
    config.dwFlags            = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle     = L"Soft Assert triggered";
    config.pszMainIcon        = TD_WARNING_ICON;
    config.pszMainInstruction = L"Soft Assert check failed:";
    config.pszContent         = msg_wchar.data();

    TASKDIALOG_BUTTON buttons[] = {
        {100, L"Skip" },
        {101, L"Break"},
        {102, L"Abort"}
    };
    config.pButtons = buttons;
    config.cButtons = COUNT_OF(buttons);

    int clicked = 0;
    TaskDialogIndirect(&config, &clicked, nullptr, nullptr);

    switch (clicked)
    {
    case 100 :
        break;
    case 102 :
        std::abort();
    default :
    case 101 :
        SA_DEBUG_BREAK();
        break;
    }
#endif
}
