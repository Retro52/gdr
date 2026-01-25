#pragma once

void sa_show_assert_popup(const char* message);

// __asm__ instructions used from ImGui
#ifndef SA_DEBUG_BREAK
#if defined(_MSC_VER)
#define SA_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
#define SA_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define SA_DEBUG_BREAK() __asm__ volatile("int3;nop")
#elif defined(__GNUC__) && defined(__thumb__)
#define SA_DEBUG_BREAK() __asm__ volatile(".inst 0xde01")
#elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
#define SA_DEBUG_BREAK() __asm__ volatile(".inst 0xe7f001f0")
#else
#define SA_DEBUG_BREAK() assert(false, "triggered an assertion because compiler is unknown")
#endif
#endif

#define soft_assert(EXPR, MESSAGE) if (!EXPR) { sa_show_assert_popup(MESSAGE); }
