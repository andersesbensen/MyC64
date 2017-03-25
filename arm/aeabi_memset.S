//===-- aeabi_memset.S - EABI memset implementation -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


//  void __aeabi_memset(void *dest, size_t n, int c) { memset(dest, c, n); }
//  void __aeabi_memclr(void *dest, size_t n) { __aeabi_memset(dest, n, 0); }
        .syntax unified
        .p2align 2
        .text

		.global __aeabi_memset
__aeabi_memset:
        mov     r3, r1
        mov     r1, r2
        mov     r2, r3
        b       memset

		.global __aeabi_memset4
		.set __aeabi_memset4,__aeabi_memset

		.global __aeabi_memset8
		.set __aeabi_memset8,__aeabi_memset

		.global __aeabi_memclr
__aeabi_memclr:
        mov     r2, r1
        movs    r1, #0
        b       memset

		.global __aeabi_memclr4
		.set __aeabi_memclr4 ,__aeabi_memclr


        .global memset
        .thumb
        .thumb_func
        .type   memset, %function

memset:
        @ args = 0, pretend = 0, frame = 0
        @ frame_needed = 0, uses_anonymous_args = 0
        @ link register save eliminated.
        mov     r3, r2
        cbz     r1, .L3
        mov     r2, r1
        uxtb    r1, r3
        b       memset
.L3:
        bx      lr
        .size   memset, .-memset
        .ident  "GCC: (GNU) 5.1.0"

