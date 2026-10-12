.align 4
.global _start, _dyld_patch_info
.section __TEXT,__text

_start:
    b       _main

#include "util.s"
#include "jbserver.s"
#include "codesign.s"

_main:
    daddr   x16, ___errno_symbol
    add     x16, x16, #0x4
    mov     x8, #0xffffffff
    str     w8, [x16]

    mov     x28, sp
    and     sp, x28, #~(16-1)
    sub     sp, sp, #0x20
    str     x28, [sp]
    mov     fp, xzr

    mov     x16, SYS_getpid
    svc     #0x80
    cmp     w0, #1
    ble     L_call_dyld_start

    ldr	    x11, [x28, #0x8]
	add	    x10, x28, #0x10
	add	    x10, x10, x11, lsl #3
	add	    x0, x10, #0x8

    adr     x1, ___dyld_insert_str
    faddr   x16, ___getenv_symbol
    blr     x16
    cbz     x0, L_call_dyld_start

    adr     x1, ___base_hook_str
    faddr   x16, ___strstr_symbol
    blr     x16
    cbz     x0, L_call_dyld_start

    faddr   x16, ___mach_init_symbol
    blr     x16

    bl      _jbserver_init_process
    cbnz    w0, L_call_dyld_start

    daddr   x16, ___errno_symbol
    add     x16, x16, #0x4
    str     wzr, [x16]

L_call_dyld_start:
    faddr   x16, ___dyld_start_symbol
    ldr     x28, [sp]
    mov     sp, x28

    mov     x0, xzr
    mov     x1, xzr
    mov     x8, xzr
    mov     x9, xzr
    mov     x27, xzr
    mov     x28, sp
    and     sp, x28, #~(16-1)
    mov     fp, xzr
    br      x16
    brk     #1


_dyld_patch_info:
___mach_init_symbol:        .long 0x0
___cc_sha1_symbol:          .long 0x0
___cc_sha256_symbol:        .long 0x0
___cc_sha384_symbol:        .long 0x0
___ccdigest_init_symbol:    .long 0x0
___ccdigest_update_symbol:  .long 0x0
___cc_clear_symbol:         .long 0x0
___getenv_symbol:           .long 0x0
___strstr_symbol:           .long 0x0
___dyld_start_symbol:       .long 0x0
___mach_task_self_symbol:   .long 0x0
___errno_symbol:            .long 0x0
___sb_ext_offset:           .long 0x0
___page_size:               .long 0x0 // static
___arm64e_host:             .long 0x0 // static
___ios_version:             .long 0x0 // static
___ns_init_platform_symbol: .quad 0x0 // dsc (Foundation)
___use_dyld3_symbol:        .quad 0x0 // dsc (libdyld)
___dyld_insert_str:         .space 32
___base_hook_str:           .space 32
___sb_policy_str:           .space 32
