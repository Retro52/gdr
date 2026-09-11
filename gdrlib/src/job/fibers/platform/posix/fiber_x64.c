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
// Changes to the original file marl/src/osfiber_x64.c (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__x86_64__)

#include <job/fibers/fiber_api.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct fiber_registers
{
    // callee-saved registers
    uintptr_t rbx;
    uintptr_t rbp;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;

    // parameter registers
    uintptr_t rdi;
    uintptr_t rsi;

    // stack and instruction registers
    uintptr_t rsp;
    uintptr_t rip;
};

void fiber_bridge(void (*target)(void*), void* arg)
{
    target(arg);
}

fiber_registers_t* fiber_init(void* stack, uint32_t stack_size, void (*target)(void*), void* arg)
{
    fiber_registers_t* reg = malloc(sizeof(fiber_registers_t));
    memset(stack, 0, stack_size);

    uintptr_t* stack_top = (uintptr_t*)((uint8_t*)(stack) + (uint32_t)stack_size);
    reg->rip = (uintptr_t)&fiber_bridge;
    reg->rdi = (uintptr_t)target;
    reg->rsi = (uintptr_t)arg;
    reg->rsp = (uintptr_t)&stack_top[-3];
    stack_top[-2] = 0;  // No return target.

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
