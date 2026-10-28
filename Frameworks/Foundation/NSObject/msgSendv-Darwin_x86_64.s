/*
 * NSobjc_msgSendv for Darwin x86_64
 * Copyright (c) 2026 antonov. All rights reserved.
 */

                .globl  _NSobjc_msgSendv
                .text
                .p2align 4
_NSobjc_msgSendv:
                pushq   %rbp
                movq    %rsp, %rbp
                pushq   %r12
                pushq   %r13
                pushq   %r14
                pushq   %r15
                pushq   %rbx
                subq    $24, %rsp

                # Arguments:
                # %rdi = id target
                # %rsi = SEL selector
                # %rdx = size_t argFrameSize
                # %rcx = const void *argFrame

                movq    %rdi, -48(%rbp)
                movq    %rsi, -56(%rbp)
                movq    %rdx, %r14              # r14 = argFrameSize
                movq    %rcx, %r15              # r15 = argFrame

                # Lookup IMP: class_getMethodImplementation(object_getClass(target), selector)
                call    _object_getClass
                movq    %rax, %rdi
                movq    -56(%rbp), %rsi
                call    _class_getMethodImplementation
                movq    %rax, %r12              # r12 = IMP

                testq   %r12, %r12
                jz      Ldone

                # Default rdi and rsi to target and selector
                movq    -48(%rbp), %rdi
                movq    -56(%rbp), %rsi
                xorq    %rdx, %rdx
                xorq    %rcx, %rcx
                xorq    %r8, %r8
                xorq    %r9, %r9

                testq   %r15, %r15
                jz      Ldo_call

                # arg0 (self)
                cmpq    $8, %r14
                jb      Ldo_call
                movq    0(%r15), %rdi

                # arg1 (selector)
                cmpq    $16, %r14
                jb      Ldo_call
                movq    8(%r15), %rsi

                # arg2
                cmpq    $24, %r14
                jb      Ldo_call
                movq    16(%r15), %rdx

                # arg3
                cmpq    $32, %r14
                jb      Ldo_call
                movq    24(%r15), %rcx

                # arg4
                cmpq    $40, %r14
                jb      Ldo_call
                movq    32(%r15), %r8

                # arg5
                cmpq    $48, %r14
                jb      Ldo_call
                movq    40(%r15), %r9

                # If more than 6 arguments (argFrameSize > 48):
                cmpq    $48, %r14
                jbe     Ldo_call

                # Stack arguments: argFrame[6..] in reverse order
                movq    %r14, %rbx
                andq    $~7, %rbx               # round down to multiple of 8
                # Ensure 16-byte stack alignment
                movq    %rbx, %rax
                subq    $48, %rax               # bytes to push on stack
                testq   $8, %rax
                jz      Lpush_args
                pushq   $0                      # alignment padding

Lpush_args:
                subq    $8, %rbx
                cmpq    $48, %rbx
                jb      Ldo_call
                pushq   (%r15, %rbx)
                jmp     Lpush_args

Ldo_call:
                xorl    %eax, %eax              # no vector registers for varargs
                call    *%r12

Ldone:
                leaq    -40(%rbp), %rsp
                popq    %rbx
                popq    %r15
                popq    %r14
                popq    %r13
                popq    %r12
                popq    %rbp
                ret
