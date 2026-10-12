
_jbserver_get_reply_port:
    mov     x16, MACH_TRAP_reply_port
    svc     #0x80
    ret


_jbserver_close_reply_port:
    cbz     x0, 0f
    mov     x1, x0
    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]
    mov     x16, MACH_TRAP_port_deallocate
    svc     #0x80
0:
    ret


_jbserver_get_bootstrap_port_special:
    sub     sp, sp, #0x60
    stp     x29, x30, [sp, #0x50]
    add     x29, sp, #0x50

    mov     w0, #0x1513
    str     w0, [sp]
    mov     w0, #0x24
    str     w0, [sp, #0x4]

    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]
    str     w0, [sp, #0x8]

    bl      _jbserver_get_reply_port
    cbz     w0, 2f
    mov     w4, w0
    str     w0, [sp, #0xc]
    str     wzr, [sp, #0x10]

    mov     w0, #0xd51
    str     w0, [sp, #0x14]
    mov     x0, #0x100000000
    str     x0, [sp, #0x18]
    mov     w0, #0x4
    str     w0, [sp, #0x20]

    mov     x0, sp
    mov     x1, #0x3
    mov     x2, #0x24
    mov     x3, #0x30
    mov     x5, xzr
    mov     x6, xzr

    mov     x16, MACH_TRAP_mach_msg
    svc     #0x80
    cbnz    x0, 0f

    ldr     w0, [sp, #0x1c]
    b       1f
0:
    mov     x0, xzr
1:
    cbz     w4, 2f
    str     w0, [sp]
    mov     w0, w4
    bl      _jbserver_close_reply_port
    ldr     w0, [sp]
2:
    ldp     x29, x30, [sp, #0x50]
    add     sp, sp, #0x60
    ret


_jbserver_get_bootstrap_port_lookup:
    sub     sp, sp, #0x80
    stp     x29, x30, [sp, #0x70]
    add     x29, sp, #0x80

    mov     w0, #0x1513
    str     w0, [sp]
    mov     w0, #0x18
    str     w0, [sp, #0x4]

    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]
    mov     w9, w0
    str     w0, [sp, #0x8]

    bl      _jbserver_get_reply_port
    cbz     w0, 5f
    mov     w4, w0
    str     w0, [sp, #0xc]
    str     wzr, [sp, #0x10]

    mov     w0, #0xd4c
    str     w0, [sp, #0x14]

    mov     x0, sp
    mov     x1, #0x3
    mov     x2, #0x18
    mov     x3, #0x40
    mov     x5, xzr
    mov     x6, xzr

    mov     x16, MACH_TRAP_mach_msg
    svc     #0x80
    cbnz    x0, 3f

    ldr     x8, [sp, #0x1c]
    cbz     x8, 3f
    ldr     w0, [x8]
    str     w0, [sp]

    mov     w2, #1
    ldr     w3, [sp, #0x34]
0:
    cmp     w3, w2
    beq     2f
    ldr     w1, [x8, x2, lsl #2]
    cbz     w1, 1f
    
    mov     x0, x9
    mov     x16, MACH_TRAP_port_deallocate
    svc     #0x80
1:
    add     w2, w2, #1
    b       0b
2:
    mov     x0, x9
    mov     x1, x8
    ldr     w2, [sp, #0x34]
    lsl     w2, w2, #2

    mov     x16, MACH_TRAP_vm_deallocate
    svc     #0x80
    ldr     w0, [sp]
    b       4f
3:
    mov     x0, xzr
4:
    cbz     w4, 5f
    str     w0, [sp]
    mov     w0, w4
    bl      _jbserver_close_reply_port
    ldr     w0, [sp]
5:
    ldp     x29, x30, [sp, #0x70]
    add     sp, sp, #0x80
    ret


_jbserver_get_bootstrap_port:
    sub     sp, sp, #0x20
    stp     x29, x30, [sp, #0x10]
    add     x29, sp, #0x10

    bl      _jbserver_get_bootstrap_port_special
    cbnz    w0, 0f
    bl      _jbserver_get_bootstrap_port_lookup
0:
    ldp     x29, x30, [sp, #0x10]
    add     sp, sp, #0x20
    ret


_jbserver_close_bootstrap_port:
    cbz     w0, 0f
    mov     w9, w0

    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]

    mov     w1, w9
    mov     x16, MACH_TRAP_port_deallocate
    svc     #0x80
0:
    ret


_jbserver_send_msg:
    sub     sp, sp, #0x80
    stp     x29, x30, [sp, #0x70]
    add     x29, sp, #0x80

    str     x0, [sp, #0x68]
    mov     x8, x0
    mov     w1, #0x1513
    str     w1, [x8]
    str     wzr, [x8, #0x10]

    mov32   x1, JBSERVER_MSG_ID
    str     w1, [x8, #0x14]
    mov32   x1, JBSERVER_MSG_MAGIC
    str     w1, [x8, #0x18]

    mov     x16, SYS_getpid
    svc     #0x80
    str     w0, [x8, #0x1c]

    bl      _jbserver_get_reply_port
    cbz     w0, 0f
    str     w0, [sp, #0x60]
    ldr     x8, [sp, #0x68]
    str     w0, [x8, #0xc]

    bl      _jbserver_get_bootstrap_port
    cbz     w0, 0f
    str     w0, [sp, #0x64]
    ldr     x8, [sp, #0x68]
    str     w0, [x8, #0x8]

    mov     x0, x8
    mov     x1, #0x1
    ldr     w2, [x8, #0x4]
    mov     x3, xzr
    mov     x4, xzr
    mov     x5, xzr
    mov     x6, xzr

    mov     x16, MACH_TRAP_mach_msg
    svc     #0x80
    cbnz    x0, 0f

    mov     w3, #0x34
    str     w3, [sp, #0x4]
    mov     x0, sp
    mov     w1, #0x2
    mov     x2, xzr
    ldr     w4, [sp, #0x60]
    mov     x5, xzr
    mov     x6, xzr

    mov     x16, MACH_TRAP_mach_msg
    svc     #0x80
    cbnz    x0, 0f
    ldr     w0, [sp, #0x18]
    str     w0, [sp]
    b       1f
0:
    mov     x0, JBSERVER_ERR_CLIENT_FAILURE
    str     w0, [sp]
1:
    ldr     w0, [sp, #0x60]
    bl      _jbserver_close_reply_port
    ldr     w0, [sp, #0x64]
    bl      _jbserver_close_bootstrap_port

    ldr     w0, [sp]
    ldp     x29, x30, [sp, #0x70]
    add     sp, sp, #0x80
    ret


_jbserver_trustcache:
    sub     sp, sp, #0x50
    stp     x29, x30, [sp, #0x40]
    add     x29, sp, #0x40

    str     w0, [sp, #0x24]
    ldp     x2, x3, [x1]
    stp     x2, x3, [sp, #0x28]
    ldr     w2, [x1, #0x10]
    str     w2, [sp, #0x38]

    mov     w0, #0x3c
    str     w0, [sp, #0x4]
    mov     w0, JBSERVER_CMD_TRUSTCACHE
    str     w0, [sp, #0x20]
    mov     x0, sp
    bl      _jbserver_send_msg

    ldp     x29, x30, [sp, #0x40]
    add     sp, sp, #0x50
    ret


_jbserver_init_process:
    sub     sp, sp, #0x40
    stp     x29, x30, [sp, #0x30]
    add     x29, sp, #0x30

    mov     w0, #0x24
    str     w0, [sp, #0x4]
    mov     w0, JBSERVER_CMD_INIT_PROCESS
    str     w0, [sp, #0x20]
    mov     x0, sp
    bl      _jbserver_send_msg

    ldp     x29, x30, [sp, #0x30]
    add     sp, sp, #0x40
    ret