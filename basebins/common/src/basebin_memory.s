#define VM_PROT_READ        0x0001
#define VM_PROT_WRITE       0x0002
#define VM_PROT_EXECUTE     0x0004
#define VM_PROT_COPY        0x0010
#define VM_FLAGS_ANYWHERE   0x0001
#define VM_FLAGS_FIXED      0x0000

.align 4
.global _mem_set_prot, _mem_set_rw, _mem_set_rx, _mem_allocate, _mem_allocate_fixed
.global _mem_deallocate, _mem_copy, _mem_clear_cache, _mem_task_self

_mem_set_prot:
    mov     x4, x2
    mov     x2, x1
    mov     x1, x0
    adrp    x0, _mem_task_self@PAGE
    add     x0, x0, _mem_task_self@PAGEOFF
    ldr     w0, [x0]

    mov     x3, xzr
    mov     x16, #-14
    svc     #0x80
    ret
    

_mem_set_rw:
    mov     w2, (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY)
    b       _mem_set_prot


_mem_set_rx:
    mov     w2, (VM_PROT_READ|VM_PROT_EXECUTE)
    b       _mem_set_prot


_mem_allocate_internal:
    sub     sp, sp, #0x20
    stp     x29, x30, [sp, #0x10]
    add     x29, sp, #0x10

    mov     x3, x2
    mov     x2, x1
    str     x0, [sp]

    adrp    x0, _mem_task_self@PAGE
    add     x0, x0, _mem_task_self@PAGEOFF
    ldr     w0, [x0]

    mov     x1, sp
    mov     x16, #-10
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
    mov     w2, VM_FLAGS_ANYWHERE
    b       _mem_allocate_internal


_mem_allocate_fixed:
    mov     w2, VM_FLAGS_FIXED
    b       _mem_allocate_internal



_mem_deallocate:
    mov     x2, x1
    mov     x1, x0
    adrp    x0, _mem_task_self@PAGE
    add     x0, x0, _mem_task_self@PAGEOFF
    ldr     w0, [x0]

    mov     x16, #-12
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
    

_mem_clear_cache:
    cbz     x0, 1f
    cbz     x1, 1f
    and     x8, x0, #0xffffffffffffffc0
    and     x9, x0, #0x3f
    add     x9, x1, x9
    sub     x9, x9, #0x1
    mov     x10, #-0x1
    eor     x9, x10, x9, lsr #6
0:
    add     x8, x8, #0x40
    add     x9, x9, #0x1
    cbnz    x9, 0b
    dsb     ish
    isb
    ret
1:
    ic      ialluis
    dsb     sy
    isb     sy
    ret

.section __DATA,__data
_mem_task_self:     .long 0x0