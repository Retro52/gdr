#pragma once

// clang-format off
#if !defined(_WIN32) \
    && !defined(__x86_64__) \
    && !defined(__i386__) \
    && !defined(__arm__) \
    && !defined(__aarch64__) \
    && !defined(__EMSCRIPTEN__)
#error "Fibers not implemented for selected platform. See supported ABIs above."
#endif
// clang-format on

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    struct fiber_registers;
    typedef struct fiber_registers fiber_registers_t;

    void fiber_swap(fiber_registers_t* from, const fiber_registers_t* to);

    fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*entry)(void*), void* arg);

    void fiber_destroy(fiber_registers_t* fiber);

    fiber_registers_t* fiber_init_manager(void);

    void fiber_destroy_manager(fiber_registers_t* manager);

#ifdef __cplusplus
}  // extern "C"
#endif
