.align 4
.global _sys_ptrace, _sys_csops, _sys_csops_audittoken, _sys_execve

_sys_ptrace:
    mov     x16, #26
    svc     #0x80
    b.cc    0f
    stp     x29, x30, [sp, #-0x10]!
    mov     x29, sp
    bl      _cerror_nocancel
    mov     sp, x29
    ldp     x29, x30, [sp], #0x10
0:
    ret


_sys_csops:
    mov     x16, #169
    svc     #0x80
    b.cc    0f
    stp     x29, x30, [sp, #-0x10]!
    mov     x29, sp
    bl      _cerror_nocancel
    mov     sp, x29
    ldp     x29, x30, [sp], #0x10
0:
    ret


_sys_csops_audittoken:
    mov     x16, #170
    svc     #0x80
    b.cc    0f
    stp     x29, x30, [sp, #-0x10]!
    mov     x29, sp
    bl      _cerror_nocancel
    mov     sp, x29
    ldp     x29, x30, [sp], #0x10
0:
    ret


_sys_execve:
    mov     x16, #59
    svc     #0x80
    b.cc    0f
    stp     x29, x30, [sp, #-0x10]!
    mov     x29, sp
    bl      _cerror_nocancel
    mov     sp, x29
    ldp     x29, x30, [sp], #0x10
0:
    ret