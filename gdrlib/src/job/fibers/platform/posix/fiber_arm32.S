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
// Changes to the original file marl/src/osfiber_asm_arm.S (Last updated: 10.05.2026):
// Function signatures, variables, macros and code style were adapted to be used in the project

#if defined(__arm__)

#define REG_OFFSET_R0  0x00
#define REG_OFFSET_R1  0x04
#define REG_OFFSET_R12 0x08
#define REG_OFFSET_R4  0x0c
#define REG_OFFSET_R5  0x10
#define REG_OFFSET_R6  0x14
#define REG_OFFSET_R7  0x18
#define REG_OFFSET_R8  0x1c
#define REG_OFFSET_R9  0x20
#define REG_OFFSET_R10 0x24
#define REG_OFFSET_R11 0x28
#define REG_OFFSET_V8  0x2c
#define REG_OFFSET_V9  0x30
#define REG_OFFSET_V10 0x34
#define REG_OFFSET_V11 0x38
#define REG_OFFSET_V12 0x3c
#define REG_OFFSET_V13 0x40
#define REG_OFFSET_V14 0x44
#define REG_OFFSET_V15 0x48
#define REG_OFFSET_SP  0x4c
#define REG_OFFSET_LR  0x50

// x0: from
// x1: to
.text
.global fiber_swap
.align 4
.type fiber_swap, %function
fiber_swap:
    // Save context 'from'
    // TODO: multiple registers can be stored in a single instruction with: stm rA, {rB-rC}

    // Store special purpose registers
    str r12, [r0, #REG_OFFSET_R12]

    // Store callee-preserved registers
    str r4, [r0, #REG_OFFSET_R4]
    str r5, [r0, #REG_OFFSET_R5]
    str r6, [r0, #REG_OFFSET_R6]
    str r7, [r0, #REG_OFFSET_R7]
    str r8, [r0, #REG_OFFSET_R8]
    str r9, [r0, #REG_OFFSET_R9]
    str r10, [r0, #REG_OFFSET_R10]
    str r11, [r0, #REG_OFFSET_R11]

    // Store sp, lr and pc
    str sp, [r0, #REG_OFFSET_SP]
    str lr, [r0, #REG_OFFSET_LR]

    // Load context 'to'
    // TODO: multiple registers can be loaded in a single instruction with: ldm rA, {rB-rC}
    mov r3, r1

    // Load special purpose registers
    ldr r12, [r3, #REG_OFFSET_R12]

    // Load callee-preserved registers
    ldr r4, [r3, #REG_OFFSET_R4]
    ldr r5, [r3, #REG_OFFSET_R5]
    ldr r6, [r3, #REG_OFFSET_R6]
    ldr r7, [r3, #REG_OFFSET_R7]
    ldr r8, [r3, #REG_OFFSET_R8]
    ldr r9, [r3, #REG_OFFSET_R9]
    ldr r10, [r3, #REG_OFFSET_R10]
    ldr r11, [r3, #REG_OFFSET_R11]

    // Load parameter registers
    ldr r0, [r3, #REG_OFFSET_R0]
    ldr r1, [r3, #REG_OFFSET_R1]

    // Load sp, lr and pc
    ldr sp, [r3, #REG_OFFSET_SP]
    ldr lr, [r3, #REG_OFFSET_LR]
    mov pc, lr

#endif // defined(__arm__)
