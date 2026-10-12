#define SYS_exit                                1
#define SYS_read                                3
#define SYS_write                               4
#define SYS_open                                5
#define SYS_close                               6
#define SYS_lseek                               199
#define SYS_fcntl                               92
#define SYS_mmap                                197
#define SYS_munmap                              73
#define SYS_csops                               169
#define SYS_mac_syscall                         381
#define SYS_abort_with_payload                  521
#define SYS_getpid                              20
#define SYS_setuid                              23
#define SYS_getuid                              24
#define SYS_setgid                              181
#define SYS_getgid                              47
#define SYS_geteuid                             25
#define SYS_getegid                             43
#define SYS_lstat                               340

#define MACH_TRAP_vm_allocate                   -10
#define MACH_TRAP_vm_deallocate                 -12
#define MACH_TRAP_vm_protect                    -14
#define MACH_TRAP_port_allocate                 -16
#define MACH_TRAP_port_deallocate               -18
#define MACH_TRAP_port_mod_refs                 -19
#define MACH_TRAP_port_insert_right             -21
#define MACH_TRAP_reply_port                    -26
#define MACH_TRAP_thread_self                   -27
#define MACH_TRAP_task_self                     -28
#define MACH_TRAP_host_self                     -29
#define MACH_TRAP_mach_msg                      -31
#define MACH_TRAP_thread_switch                 -61

#define MAP_FIXED                               0x0010
#define VM_FLAGS_FIXED                          0x0000
#define VM_FLAGS_ANYWHERE                       0x0001
#define VM_FLAGS_NO_CACHE                       0x0010
#define VM_FLAGS_OVERWRITE                      0x4000
#define VM_PROT_NONE                            0x00
#define VM_PROT_READ                            0x01
#define VM_PROT_WRITE                           0x02
#define VM_PROT_EXECUTE                         0x04
#define VM_PROT_COPY                            0x10
#define VM_RW                                   (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY)
#define VM_RW_NO_COPY                           (VM_PROT_READ|VM_PROT_WRITE)
#define VM_RWX                                  (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)
#define VM_RX                                   (VM_PROT_READ|VM_PROT_EXECUTE)
#define VM_RO                                   (VM_PROT_READ)

#define F_ADDSIGS                               59
#define F_ADDFILESIGS                           61
#define F_ADDFILESIGS_RETURN                    97
#define F_CHECK_LV                              98
#define	SEEK_SET                                0
#define	SEEK_CUR                                1
#define CSSLOT_CODEDIRECTORY                    0x00000000
#define CSSLOT_ALTERNATE_CODEDIRECTORIES        0x00001000
#define CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT    0x00001005
#define CS_HASHTYPE_SHA1                        0x00000001
#define CS_HASHTYPE_SHA256                      0x00000002
#define CS_HASHTYPE_SHA256_TRUNCATED            0x00000003
#define CS_HASHTYPE_SHA384                      0x00000004

#define JBSERVER_MSG_ID                         0x40000206
#define JBSERVER_MSG_MAGIC                      0x48415858
#define JBSERVER_ERR_CLIENT_FAILURE             7
#define JBSERVER_ERR_UNKNOWN_FAILURE            9
#define JBSERVER_CMD_UNKNOWN                    0
#define JBSERVER_CMD_TRUSTCACHE                 1
#define JBSERVER_CMD_INIT_PROCESS               3

// function addr
.macro faddr reg target
    adr \reg, \target
    ldr w17, [\reg]
    sub \reg, \reg, x17
.endmacro

// data addr
.macro daddr reg target
    adr \reg, \target
    ldr w17, [\reg]
    add \reg, \reg, x17
.endmacro

.macro mov64
    movz $0, #((($1)>>00)&0xffff)
    movk $0, #((($1)>>16)&0xffff), lsl #16
    movk $0, #((($1)>>32)&0xffff), lsl #32
    movk $0, #((($1)>>48)&0xffff), lsl #48
.endmacro

.macro mov48
    movz $0, #((($1)>>00)&0xffff)
    movk $0, #((($1)>>16)&0xffff), lsl #16
    movk $0, #((($1)>>32)&0xffff), lsl #32
.endmacro

.macro mov32
    movz $0, #((($1)>>00)&0xffff)
    movk $0, #((($1)>>16)&0xffff), lsl #16
.endmacro


_mem_set_prot:
    mov     x4, x2
    mov     x2, x1
    mov     x1, x0
    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]

    mov     x3, xzr
    mov     x16, #-14
    svc     #0x80
    ret



_mem_allocate_internal:
    sub     sp, sp, #0x20
    stp     x29, x30, [sp, #0x10]
    add     x29, sp, #0x10

    mov     x3, x2
    mov     x2, x1
    str     x0, [sp]

    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]

    mov     x1, sp
    mov     x16, MACH_TRAP_vm_allocate
    svc     #0x80

    cbnz    w0, 0f
    ldr     x0, [sp]
    cbnz    x0, 1f
0:
    mov     x0, #0
1:
    ldp     x29, x30, [sp, #0x10]
    add     sp, sp, #0x20
    ret



_mem_allocate:
    mov     x1, x0
    mov     x0, xzr
    mov     x2, VM_FLAGS_ANYWHERE
    b       _mem_allocate_internal



_mem_allocate_fixed:
    mov     x2, VM_FLAGS_FIXED|VM_FLAGS_OVERWRITE
    b       _mem_allocate_internal



_mem_deallocate:
    mov     x2, x1
    mov     x1, x0
    daddr   x16, ___mach_task_self_symbol
    ldr     w0, [x16]

    mov     x16, MACH_TRAP_vm_deallocate
    svc     #0x80
    ret



_mem_copy:
    mov     x8, xzr
0:
    cmp     x2, x8
    beq     1f
    ldrb    w9, [x1, x8]
    strb    w9, [x0, x8]
    add     x8, x8, #0x1
    b       0b
1:
    ret


_strlen:
    cbz     x0, L_strlen_error
    mov     x1, x0
    mov     x0, xzr

L_strlen_loop:
    cmp     w0, #0x400
    bge     L_strlen_error
    ldrb    w2, [x1, x0]
    cbz     w2, L_strlen_done
    add     w0, w0, #0x1
    b L_strlen_loop
    
L_strlen_error:
    mov     x0, xzr

L_strlen_done:
    ret
