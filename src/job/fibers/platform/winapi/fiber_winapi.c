#if defined(_WIN32)

#include <job/fibers/fiber_api.h>
#include <stdint.h>
#include <stdlib.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct fiber_registers
{
    LPVOID handle;
};

void fiber_swap(fiber_registers_t* from, const fiber_registers_t* to)
{
    SwitchToFiber(to->handle);
}

fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*target)(void*), void* arg)
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));
    reg->handle            = CreateFiber(stack_size, target, arg);
    return reg;
}

void fiber_destroy(fiber_registers_t* reg)
{
    DeleteFiber(reg->handle);
    free(reg);
}

fiber_registers_t* fiber_init_manager()
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));
    reg->handle            = ConvertThreadToFiber(0);
    return reg;
}

void fiber_destroy_manager(fiber_registers_t* reg)
{
    free(reg);
    ConvertFiberToThread();
}

#endif
