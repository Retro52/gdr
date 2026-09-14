#if defined(__EMSCRIPTEN__)

#include <emscripten/fiber.h>
#include <job/fibers/fiber_api.h>
#include <stdint.h>
#include <stdlib.h>

struct fiber_registers
{
    emscripten_fiber_t context;
    void* asyncify_stack_ptr;
};

void fiber_swap(fiber_registers_t* from, const fiber_registers_t* to)
{
    emscripten_fiber_swap(&from->context, (emscripten_fiber_t*)&to->context);
}

fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*target)(void*), void* arg)
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));

    const uint64_t asyncify_stack_size = 1024 * 1024;

    reg->asyncify_stack_ptr = malloc(asyncify_stack_size);
    emscripten_fiber_init(&reg->context, target, arg, stack, stack_size, reg->asyncify_stack_ptr, asyncify_stack_size);
    return reg;
}

void fiber_destroy(fiber_registers_t* reg)
{
    free(reg->asyncify_stack_ptr);
    free(reg);
}

fiber_registers_t* fiber_init_manager()
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));

    const uint64_t asyncify_stack_size = 1024 * 1024;
    reg->asyncify_stack_ptr = malloc(asyncify_stack_size);

    emscripten_fiber_init_from_current_context(&reg->context, reg->asyncify_stack_ptr, asyncify_stack_size);
    return reg;
}

void fiber_destroy_manager(fiber_registers_t* reg)
{
    free(reg->asyncify_stack_ptr);
    free(reg);
}

#endif
