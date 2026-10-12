
#ifdef DYLD_PATCH_IOS_13
_cc_sha_helper:
    stp     x24, x23, [sp, #-0x40]!
    stp     x22, x21, [sp, #0x10]
    stp     x20, x19, [sp, #0x20]
    stp     x29, x30, [sp, #0x30]
    add     x29, sp, #0x30
    sub     sp, sp, #0x10
    mov     x19, x2
    mov     x20, x1
    mov     x21, x0
    blr     x16

    mov     x22, x0
    ldp     x8, x9, [x0, #0x8]
    add     x8, x8, x9
    add     x8, x8, #0x13
    and     x8, x8, #0xfffffffffffffff8
    add     x8, x8, #0xf
    and     x8, x8, #0xfffffffffffffff0
    mov     x9, sp
    sub     x23, x9, x8
    mov     sp, x23
    mov     x1, x23
    faddr   x16, ___ccdigest_init_symbol
    blr     x16

    mov     x0, x22
    mov     x1, x23
    mov     x2, x20
    mov     x3, x21
    faddr   x16, ___ccdigest_update_symbol
    blr     x16

    ldr     x8, [x22, #0x38]
    mov     x0, x22
    mov     x1, x23
    mov     x2, x19
    adr     x5, ___arm64e_host
    ldr     w5, [x5]
    cbz     w5, 0f
    .inst   0xdac143e8 // xpaci x8
0:
    blr     x8


    ldp     x8, x9, [x22, #0x8]
    add     x8, x8, x9
    add     x0, x8, #0xc
    mov     x1, x23
    faddr   x16, ___cc_clear_symbol
    blr     x16

    mov     x0, xzr
    sub     sp, x29, #0x30
    ldp     x29, x30, [sp, #0x30]
    ldp     x20, x19, [sp, #0x20]
    ldp     x22, x21, [sp, #0x10]
    ldp     x24, x23, [sp], #0x40
    ret
#endif

_calculate_cd_hash:
    sub     sp, sp, #0x20
    stp     x29, x30, [sp, #0x10]
    add     x29, sp, #0x10

    mov     x3, xzr
    ldr     w4, [x0, #0x8]
    rev     w4, w4
    mov     x8, xzr
    mov     x9, xzr
    mov     x10, xzr

L_loop_start:
    cmp     w4, w3
    beq     L_loop_end
    
    add     x6, x0, x3, lsl #3
    ldp     w6, w7, [x6, #0xc]
    rev     w6, w6
    rev     w7, w7

    cmp     w6, CSSLOT_CODEDIRECTORY
    beq     L_handle_cd
    cmp     w6, CSSLOT_ALTERNATE_CODEDIRECTORIES
    blt     L_loop_cont
    mov     w5, CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT
    cmp     w6, w5
    bge     L_loop_cont

L_handle_cd:
    add     x6, x0, x7
    ldrb    w7, [x6, #0x25]
    mov     w5, w7
    cmp     w7, CS_HASHTYPE_SHA256
    beq     0f
    cmp     w7, CS_HASHTYPE_SHA256_TRUNCATED
    beq     1f
    b       2f
0:
    mov     w7, #3
    b       2f
1:
    mov     w7, #2
2:
    cmp     w7, w8
    ble     L_loop_cont
    mov     w8, w7
    mov     x9, x6
    mov     w10, w5

L_loop_cont:
    add     w3, w3, #1
    b       L_loop_start

L_loop_end:
    cbz     x9, L_cd_error
    str     w10, [sp]
    mov     x0, x9
    ldr     w1, [x0, #0x4]
    rev     w1, w1
    cmp     w10, CS_HASHTYPE_SHA1
    beq     L_handle_sha1
    cmp     w10, CS_HASHTYPE_SHA256
    beq     L_handle_sha256
    cmp     w10, CS_HASHTYPE_SHA256_TRUNCATED
    beq     L_handle_sha256
    cmp     w10, CS_HASHTYPE_SHA384
    beq     L_handle_sha384
    b       L_cd_error

L_handle_sha1:
    faddr   x16, ___cc_sha1_symbol
#ifdef DYLD_PATCH_IOS_13
    bl      _cc_sha_helper
#else
    blr     x16
#endif
    b       L_process_done

L_handle_sha256:
    faddr   x16, ___cc_sha256_symbol
#ifdef DYLD_PATCH_IOS_13
    bl      _cc_sha_helper
#else
    blr     x16
#endif
    b       L_process_done

L_handle_sha384:
    faddr   x16, ___cc_sha384_symbol
#ifdef DYLD_PATCH_IOS_13
    bl      _cc_sha_helper
#else
    blr     x16
#endif

L_process_done:
    ldr     w0, [sp]
    b       L_cd_done

L_cd_error:
    mov     x0, #-1

L_cd_done:
    ldp     x29, x30, [sp, #0x10]
    add     sp, sp, #0x20
    ret



_fcntl_hook:
    cmp     w1, F_CHECK_LV
    beq     L_bypass_lv

    mov     x9, x0
    mov     x10, x1
    mov     x11, x2

    mov     x16, SYS_getpid
    svc     #0x80
    mov     x8, x0
    mov     x0, x9
    mov     x1, x10
    mov     x2, x11
    mov     w9, #1
    cmp     w8, w9
    ble     L_orig_fcntl

    cmp     w1, F_ADDFILESIGS
    beq     L_process
    cmp     w1, F_ADDFILESIGS_RETURN
    beq     L_process

L_orig_fcntl:
    mov     x16, SYS_fcntl
    svc     #0x80
    ret

L_bypass_lv:
    mov     x0, xzr
    ret

L_process:
    sub     sp, sp, #0x90
    stp     x29, x30, [sp, #0x80]
    stp     x2, x3, [sp, #0x70]
    stp     x0, x1, [sp, #0x60]
    add     x29, sp, #0x80

    mov     x16, SYS_fcntl
    svc     #0x80
    cbz     w0, L_hook_done

    ldr     x0, [x2, #0x10]
    cbz     x0, L_error
    str     x0, [sp, 0x58]
    bl      _mem_allocate
    cbz     x0, L_error
    str     x0, [sp, 0x50]

    ldr     x0, [sp, #0x60]
    ldr     x8, [sp, #0x70]
    ldr     x1, [x8]
    ldr     x8, [x8, #0x8]
    add     x1, x1, x8
    mov     w2, SEEK_SET
    mov     x16, SYS_lseek
    svc     #0x80

    ldr     x0, [sp, #0x60]
    ldp     x1, x2, [sp, 0x50]
    mov     x16, SYS_read
    svc     #0x80
    cmp     x0, x2
    bne     L_error

    ldr     x0, [sp, 0x50]
    mov     x1, x2
    add     x2, sp, #0x20
    bl      _calculate_cd_hash
    cbz     x0, L_error
    cmp     x0, #0x4
    bgt     L_error

    add     x1, sp, #0x20
    bl      _jbserver_trustcache
    cbnz    x0, L_error

    ldp     x2, x3, [sp, #0x70]
    ldp     x0, x1, [sp, #0x60]
    mov     x16, SYS_fcntl
    svc     #0x80

    ldr     x8, [sp, #0x68]
    cmp     w8, F_ADDFILESIGS_RETURN
    bne     0f

    ldr     x8, [sp, #0x70]
    cbz     x8, 0f
    mov     w9, #0xffffffff
    str     w9, [x8]
0:
    b       L_cleanup

L_error:
    mov     x0, #-1

L_cleanup:
    ldr     x8, [sp, 0x50]
    cbz     x8, L_hook_done
    ldr     x1, [sp, 0x58]
    cbz     x1, L_hook_done
    
    str     x0, [sp]
    mov     x0, x8
    bl      _mem_deallocate
    ldr     x0, [sp]

L_hook_done:
    ldp     x29, x30, [sp, #0x80]
    add     sp, sp, #0x90
    ret



_mmap_hook:
    sub     sp, sp, #0x60
    stp     x29, x30, [sp, #0x50]
    stp     x4, x5, [sp, #0x40]
    stp     x2, x3, [sp, #0x30]
    stp     x0, x1, [sp, #0x20]
    add     x29, sp, #0x50

    mov     x16, SYS_mmap
    svc     #0x80
    bcc     L_map_done
    str     x0, [sp, #0x8]

    mov     x9, x0
    mov     x16, SYS_getpid
    svc     #0x80
    mov     x8, x0
    mov     x0, x9
    mov     w9, #1
    cmp     w8, w9
    ble     L_map_done

    ldr     x0, [sp, #0x40]
    cmp     x0, #0
    blt     L_map_failure

    ldr     x0, [sp, #0x30]
    and     x0, x0, VM_PROT_EXECUTE
    cmp     x0, VM_PROT_EXECUTE
    bne     L_map_failure

    ldp     x0, x1, [sp, #0x20]
    bl      _mem_allocate_fixed
    cbz     x0, L_map_cleanup
    str     x0, [sp, #0x10]

    ldp     x4, x5, [sp, #0x40]
    ldp     x2, x3, [sp, #0x30]
    ldp     x0, x1, [sp, #0x20]
    and     x2, x2, #~(VM_PROT_EXECUTE)
    and     x3, x3, #~(MAP_FIXED)
    mov     x0, xzr

    mov     x16, SYS_mmap
    svc     #0x80
    bcs     L_map_failure
    str     x0, [sp]

    mov     x1, x0
    ldr     x0, [sp, #0x10]
    ldr     x2, [sp, #0x28]
    bl      _mem_copy

    ldr     x0, [sp]
    ldr     x1, [sp, #0x28]
    mov     x16, SYS_munmap
    svc     #0x80
    str     xzr, [sp]

    ldr     x0, [sp, #0x10]
    ldr     x1, [sp, #0x28]
    ldr     x2, [sp, #0x30]
    bl      _mem_set_prot
    cbnz    w0, L_map_cleanup
    ldr     x0, [sp]
    b       L_map_done

L_map_cleanup:
    ldr     x0, [sp]
    cbz     x0, 0f
    ldr     x1, [sp, #0x28]
    mov     x16, SYS_munmap
    svc     #0x80
0:
    ldr     x0, [sp, #0x10]
    cbz     x0, L_map_failure
    ldr     x1, [sp, #0x28]
    bl      _mem_deallocate
    
L_map_failure:
    ldr     x0, [sp, #0x8]
    daddr   x8, ___errno_symbol
    str     w0, [x8]
    mrs     x8, TPIDRRO_EL0
    and     x8, x8, #-0x8
    ldr     x8, [x8, #0x8]
    cbz     x8, 1f
    str     w0, [x8]
1:
    mov     x0, #-1
    mov     x1, #-1

L_map_done:
    ldp     x29, x30, [sp, #0x50]
    add     sp, sp, #0x60
    ret
