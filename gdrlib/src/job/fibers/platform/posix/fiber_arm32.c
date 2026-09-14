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
// Changes to the original file marl/src/osfiber_arm.c (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__arm__)

#include <job/fibers/fiber_api.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct fiber_registers
{
    // parameter registers
    uintptr_t r0;
    uintptr_t r1;

    // special purpose registers
    uintptr_t r12;  // Intra-Procedure-call

    // callee-saved registers
    uintptr_t r4;
    uintptr_t r5;
    uintptr_t r6;
    uintptr_t r7;
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;

    uintptr_t v8;
    uintptr_t v9;
    uintptr_t v10;
    uintptr_t v11;
    uintptr_t v12;
    uintptr_t v13;
    uintptr_t v14;
    uintptr_t v15;

    uintptr_t sp;  // stack pointer (r13)
    uintptr_t lr;  // link register (r14)
};

void fiber_bridge(void (*target)(void*), void* arg)
{
    target(arg);
}

fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*target)(void*), void* arg)
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));

    memset(stack, 0, stack_size);

    uintptr_t* stack_top = (uintptr_t*)((uint8_t*)(stack) + stack_size);
    reg->lr = (uintptr_t)&fiber_bridge;
    reg->r0 = (uintptr_t)target;
    reg->r1 = (uintptr_t)arg;
    reg->sp = ((uintptr_t)stack_top) & ~(uintptr_t)15;
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
