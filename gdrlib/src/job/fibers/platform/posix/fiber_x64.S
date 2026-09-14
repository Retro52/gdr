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
// Changes to the original file marl/src/osfiber_asm_x64.S (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__x86_64__)

#define REG_OFFSET_RBX 0x00
#define REG_OFFSET_RBP 0x08
#define REG_OFFSET_R12 0x10
#define REG_OFFSET_R13 0x18
#define REG_OFFSET_R14 0x20
#define REG_OFFSET_R15 0x28
#define REG_OFFSET_RDI 0x30
#define REG_OFFSET_RSI 0x38
#define REG_OFFSET_RSP 0x40
#define REG_OFFSET_RIP 0x48

#if defined(__APPLE__)
#define ASM_SYMBOL(x) _##x
#else
#define ASM_SYMBOL(x) x
#endif

.global ASM_SYMBOL(fiber_swap)
.align 4
ASM_SYMBOL(fiber_swap):
    // Save context 'from'

    // Store callee-preserved registers
    movq        %rbx, REG_OFFSET_RBX(%rdi)
    movq        %rbp, REG_OFFSET_RBP(%rdi)
    movq        %r12, REG_OFFSET_R12(%rdi)
    movq        %r13, REG_OFFSET_R13(%rdi)
    movq        %r14, REG_OFFSET_R14(%rdi)
    movq        %r15, REG_OFFSET_R15(%rdi)

    movq        (%rsp), %rcx             /* call stores the return address on the stack before jumping */
    movq        %rcx, REG_OFFSET_RIP(%rdi)
    leaq        8(%rsp), %rcx            /* skip the pushed return address */
    movq        %rcx, REG_OFFSET_RSP(%rdi)

    // Load context 'to'
    movq        %rsi, %r8

    // Load callee-preserved registers
    movq        REG_OFFSET_RBX(%r8), %rbx
    movq        REG_OFFSET_RBP(%r8), %rbp
    movq        REG_OFFSET_R12(%r8), %r12
    movq        REG_OFFSET_R13(%r8), %r13
    movq        REG_OFFSET_R14(%r8), %r14
    movq        REG_OFFSET_R15(%r8), %r15

    // Load first two call parameters
    movq        REG_OFFSET_RDI(%r8), %rdi
    movq        REG_OFFSET_RSI(%r8), %rsi

    // Load stack pointer
    movq        REG_OFFSET_RSP(%r8), %rsp

    // Load instruction pointer, and jump
    movq        REG_OFFSET_RIP(%r8), %rcx
    jmp         *%rcx

#endif