// Copyright 2019 The Marl Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// NOTE (Anton):
// Changes to the original file marl/src/osfiber_x86.c (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__i386__)

#include <job/fibers/fiber_api.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct fiber_registers
{
    // callee-saved registers
    uintptr_t ebx;
    uintptr_t ebp;
    uintptr_t esi;
    uintptr_t edi;

    // stack and instruction registers
    uintptr_t esp;
    uintptr_t eip;
};

void fiber_bridge(void (*target)(void*), void* arg)
{
    target(arg);
}

fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*target)(void*), void* arg)
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));
    memset(stack, 0, stack_size);

    // NOTE (Anton): Original comment belongs to MARL authors. Function signature was changed from marl_fiber_set_target to fiber_init for
    // clarity. The stack pointer needs to be 16-byte aligned when making a 'call'. The 'call' instruction automatically pushes the return
    // instruction to the stack (4-bytes), before making the jump. The fiber_init() assembly function does not use 'call', instead it uses
    // 'jmp', so we need to offset the ESP pointer by 4 bytes so that the stack is still 16-byte aligned when the return target is
    // stack-popped by the callee.
    uintptr_t* stack_top = (uintptr_t*)((uint8_t*)(stack) + stack_size);
    reg->eip = (uintptr_t)&fiber_bridge;
    reg->esp = (uintptr_t)&stack_top[-5];
    stack_top[-3] = (uintptr_t)arg;
    stack_top[-4] = (uintptr_t)target;
    stack_top[-5] = 0;  // No return target.
    return reg;
}

void fiber_destroy(fiber_registers_t* reg)
{
    free(reg);
}

fiber_registers_t* fiber_init_manager()
{
    return malloc(sizeof(fiber_registers_t));
}

void fiber_destroy_manager(fiber_registers_t* reg)
{
    free(reg);
}

#endif
