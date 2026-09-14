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
// Changes to the original file marl/src/osfiber_asm_x86.S (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__i386__)

#define REG_OFFSET_EBX 0x00
#define REG_OFFSET_EBP 0x04
#define REG_OFFSET_ESI 0x08
#define REG_OFFSET_EDI 0x0c
#define REG_OFFSET_ESP 0x10
#define REG_OFFSET_EIP 0x14

#if defined(__APPLE__)
#define ASM_SYMBOL(x) _##x
#else
#define ASM_SYMBOL(x) x
#endif

.global ASM_SYMBOL(fiber_swap)
.align 4
ASM_SYMBOL(fiber_swap):
    // Save context 'from'
    movl        4(%esp), %eax

    // Store callee-preserved registers
    movl        %ebx, REG_OFFSET_EBX(%eax)
    movl        %ebp, REG_OFFSET_EBP(%eax)
    movl        %esi, REG_OFFSET_ESI(%eax)
    movl        %edi, REG_OFFSET_EDI(%eax)

    movl        (%esp), %ecx             /* call stores the return address on the stack before jumping */
    movl        %ecx, REG_OFFSET_EIP(%eax)
    lea         4(%esp), %ecx            /* skip the pushed return address */
    movl        %ecx, REG_OFFSET_ESP(%eax)

    // Load context 'to'
    movl        8(%esp), %ecx

    // Load callee-preserved registers
    movl        REG_OFFSET_EBX(%ecx), %ebx
    movl        REG_OFFSET_EBP(%ecx), %ebp
    movl        REG_OFFSET_ESI(%ecx), %esi
    movl        REG_OFFSET_EDI(%ecx), %edi

    // Load stack pointer
    movl        REG_OFFSET_ESP(%ecx), %esp

    // Load instruction pointer, and jump
    movl        REG_OFFSET_EIP(%ecx), %ecx
    jmp         *%ecx
#endif