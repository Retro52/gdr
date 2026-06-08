#pragma once

#include <quill/LogMacros.h>
#include <quill/SimpleSetup.h>

namespace logging
{
    extern quill::Logger* s_instance;
}

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_NOTICE
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL
#undef LOG_BACKTRACE

#define LOG_DEBUG(fmt, ...)     QUILL_LOG_DEBUG(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_INFO(fmt, ...)      QUILL_LOG_INFO(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_NOTICE(fmt, ...)    QUILL_LOG_NOTICE(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_WARNING(fmt, ...)   QUILL_LOG_WARNING(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_ERROR(fmt, ...)     QUILL_LOG_ERROR(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_CRITICAL(fmt, ...)  QUILL_LOG_CRITICAL(logging::s_instance, fmt, ##__VA_ARGS__);
#define LOG_BACKTRACE(fmt, ...) QUILL_LOG_BACKTRACE(logging::s_instance, fmt, ##__VA_ARGS__);
