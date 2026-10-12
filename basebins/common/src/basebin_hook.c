#include "basebin_instructions.h"
#include "basebin_memory.h"
#include "basebin_util.h"
#include "basebin_hook.h"

static int init_hook_info(void *target, void *replacement, bool original_needed, hook_info_t *info) {
    if (target == NULL || replacement == NULL) return -1;
    info->type = HOOK_TYPE_NONE;
    info->size = 0;
    info->requires_relocation = false;

    uint8_t replaceable = a64_replaceable_count(target, false, false, !original_needed);
    uint8_t relocatable = a64_replaceable_count(target, true, true, !original_needed);
    uint8_t count = relocatable;

    if (count >= 1 && in_branch_range(target, replacement)) {
        if (original_needed) info->requires_relocation = (replaceable < 1);
        info->type = HOOK_TYPE_DIRECT_BRANCH;
        info->size = 0x4;
        return 0;
    }

    if (count >= 3 && in_adrp_range(target, replacement)) {
        if (original_needed) info->requires_relocation = (replaceable < 3);
        info->type = HOOK_TYPE_ADRP_ADD_BR;
        info->size = 0xc;
        return 0;
    }

    if (count >= 3 && (((uint64_t)replacement & 0x0000ffff0000ffff) == (uint64_t)replacement)) {
        if (original_needed) info->requires_relocation = (replaceable < 3);
        info->type = HOOK_TYPE_MOV2_BR;
        info->size = 0xc;
        return 0;
    }

    if (count >= 3 && (((uint64_t)replacement & 0x0000ffffffff0000) == (uint64_t)replacement)) {
        if (original_needed) info->requires_relocation = (replaceable < 3);
        info->type = HOOK_TYPE_ALT_MOV2_BR;
        info->size = 0xc;
        return 0;
    }

    if (count >= 4 && (((uint64_t)replacement & 0x0000ffffffffffff) == (uint64_t)replacement)) {
        if (original_needed) info->requires_relocation = (replaceable < 4);
        info->type = HOOK_TYPE_MOV3_BR;
        info->size = 0x10;
        return 0;
    }

    if (count >= 5) {
        if (original_needed) info->requires_relocation = (replaceable < 5);
        info->type = HOOK_TYPE_MOV4_BR;
        info->size = 0x14;
        return 0;
    }

    if (count >= 4) {
        if (original_needed) info->requires_relocation = (replaceable < 4);
        info->type = HOOK_TYPE_LDR_BR;
        info->size = 0x10;
        return 0;
    }

    if (count >= 2 && in_adrp_range(target, replacement) && ARM_PAGE_OF(replacement) == (uint64_t)replacement) {
        if (original_needed) info->requires_relocation = (replaceable < 2);
        info->type = HOOK_TYPE_ADRP_BR;
        info->size = 0x8;
        return 0;
    }
    return (info->type == HOOK_TYPE_NONE) ? -1 : 0;
}

static void install_adr_relocation(void *from, void *to, uint32_t inst) {
    uint64_t imm = ((inst >> 3) & 0x1ffffc) | ((inst >> 29) & 0x3);
    uint64_t base = 0x0;
    uint8_t sign = 0;

    if ((inst & A64_ADRP_MASK) == A64_ADRP_OPCODE) {
        base = (uint64_t)ARM_PAGE_OF(from);
        sign = 31;
        imm <<= 12;
    } else {
        base = (uint64_t)from;
        sign = 43;
    }

    uint64_t location = base + (((int64_t)((imm) << sign)) >> sign);
    uint8_t reg = (inst & 0x1f);
    uint32_t *data = (uint32_t *)to;

    data[0] = a64_gen_movz(reg, location & 0xffff, 0);
    data[1] = a64_gen_movk(reg, (location >> 16) & 0xffff, 16);
    data[2] = a64_gen_movk(reg, (location >> 32) & 0xffff, 32);
    data[3] = a64_gen_movk(reg, (location >> 48) & 0xffff, 48);
}

static void install_branch_relocation(void *from, void *to, uint32_t inst, uint8_t reg) {
    uint32_t type = -1;
    if ((inst & A64_B_MASK) == A64_B_OPCODE) type = A64_B_OPCODE;
    else if ((inst & A64_BL_MASK) == A64_BL_OPCODE) type = A64_BL_OPCODE;
    else if ((inst & A64_B_COND_MASK) == A64_B_COND_OPCODE) type = A64_B_COND_OPCODE;

    int64_t offset = 0;
    uint8_t cond = 0;
    bool link = false;

    if (type == A64_B_OPCODE || type == A64_BL_OPCODE) {
        offset = ((((int64_t)((uint64_t)(inst & 0x3ffffff) << 38)) >> 38) * 0x4);
        if (type == A64_BL_OPCODE) link = true;
    } else {
        offset = ((((int64_t)((uint64_t)((inst >> 5) & 0x7ffff) << 45)) >> 45) * 0x4);
        cond = (inst & 0xf);
    }

    uint64_t location = (uint64_t)from + offset;
    uint32_t *data = (uint32_t *)to;

    data[0] = a64_gen_movz(reg, location & 0xffff, 0);
    data[1] = a64_gen_movk(reg, (location >> 16) & 0xffff, 16);
    data[2] = a64_gen_movk(reg, (location >> 32) & 0xffff, 32);
    data[3] = a64_gen_movk(reg, (location >> 48) & 0xffff, 48);

    if (cond == 0) {
        data[4] = link ? a64_gen_blr(reg) : a64_gen_br(reg);
    } else {
        data[4] = a64_gen_b_cond(0x8, cond);
        data[5] = a64_gen_b(0x8);
        data[6] = a64_gen_br(reg);
    }
}

static void install_compare_branch_relocation(void *from, void *to, uint32_t inst, uint8_t reg) {
    uint32_t type = -1;
    if ((inst & A64_CBZ_W_MASK) == A64_CBZ_W_OPCODE) type = A64_CBZ_W_OPCODE;
    else if ((inst & A64_CBZ_X_MASK) == A64_CBZ_X_OPCODE) type = A64_CBZ_X_OPCODE;
    else if ((inst & A64_CBNZ_W_MASK) == A64_CBNZ_W_OPCODE) type = A64_CBNZ_W_OPCODE;
    else if ((inst & A64_CBNZ_X_MASK) == A64_CBNZ_X_OPCODE) type = A64_CBNZ_X_OPCODE;

    uint8_t compare_reg = (inst & 0x1f);
    uint8_t compare_sf = ((inst >> 31) & 0x1);
    int64_t offset = (((int64_t)(((uint64_t)(((inst >> 5) & 0x7ffff) << 2)) << 45)) >> 45);
    uint64_t location = (uint64_t)from + offset;
    uint32_t *data = (uint32_t *)to;

    data[0] = a64_gen_movz(reg, location & 0xffff, 0);
    data[1] = a64_gen_movk(reg, (location >> 16) & 0xffff, 16);
    data[2] = a64_gen_movk(reg, (location >> 32) & 0xffff, 32);
    data[3] = a64_gen_movk(reg, (location >> 48) & 0xffff, 48);
        
    if (type == A64_CBZ_W_OPCODE || type == A64_CBZ_X_OPCODE) {
        data[4] = a64_gen_cbz(compare_sf, compare_reg, 0x8);
    } else {
        data[4] = a64_gen_cbnz(compare_sf, compare_reg, 0x8);
    }

    data[5] = a64_gen_b(0x8);
    data[6] = a64_gen_br(reg);
}

static void install_hook(void *from, void *to, hook_type_t type, uint8_t reg) {
    switch (type) {
        case HOOK_TYPE_DIRECT_BRANCH: {
            *(uint32_t *)from = a64_gen_b(branch_difference(from, to));
        } break;

        case HOOK_TYPE_ADRP_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_adrp(reg, page_difference(from, to));
            data[1] = a64_gen_br(reg);
        } break;

        case HOOK_TYPE_ADRP_ADD_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_adrp(reg, page_difference(from, to));
            data[1] = a64_gen_add(reg, reg, ARM_PAGE_OFFSET(to), 0);
            data[2] = a64_gen_br(reg);
        } break;

        case HOOK_TYPE_LDR_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_ldr_literal(reg, 0x8);
            data[1] = a64_gen_br(reg);
            data[2] = ((uint64_t)to) & 0xffffffff;
            data[3] = ((uint64_t)to >> 32) & 0xffffffff;
        } break;

        case HOOK_TYPE_MOV2_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_movz(reg, (uint64_t)to & 0xffff, 0);
            data[1] = a64_gen_movk(reg, ((uint64_t)to >> 32) & 0xffff, 32);
            data[2] = a64_gen_br(reg);
        } break;

        case HOOK_TYPE_ALT_MOV2_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_movz(reg, ((uint64_t)to >> 16) & 0xffff, 16);
            data[1] = a64_gen_movk(reg, ((uint64_t)to >> 32) & 0xffff, 32);
            data[2] = a64_gen_br(reg);
        } break;

        case HOOK_TYPE_MOV3_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_movz(reg, (uint64_t)to & 0xffff, 0);
            data[1] = a64_gen_movk(reg, ((uint64_t)to >> 16) & 0xffff, 16);
            data[2] = a64_gen_movk(reg, ((uint64_t)to >> 32) & 0xffff, 32);
            data[3] = a64_gen_br(reg);
        } break;

        case HOOK_TYPE_MOV4_BR: {
            uint32_t *data = (uint32_t *)from;
            data[0] = a64_gen_movz(reg, (uint64_t)to & 0xffff, 0);
            data[1] = a64_gen_movk(reg, ((uint64_t)to >> 16) & 0xffff, 16);
            data[2] = a64_gen_movk(reg, ((uint64_t)to >> 32) & 0xffff, 32);
            data[3] = a64_gen_movk(reg, ((uint64_t)to >> 48) & 0xffff, 48);
            data[4] = a64_gen_br(reg);
        } break;
        default: break;
    }
}

int hook_function(void *target, void *replacement, void **original) {
    target = xpaci(target);
    replacement = xpaci(replacement);
    hook_info_t info = {0};
    if (init_hook_info(target, replacement, (original != NULL), &info) != 0) return 1;

    void *orig_base = NULL;
    uint32_t orig_size = 0;
    void *secondary_base = NULL;
    uint32_t secondary_size = 0;
    uint8_t branch_reg = 0;
    uint8_t scratch_reg = 0;

    if (a64_get_usable_registers(target, info.size/4, &branch_reg, &scratch_reg) != 0) return 2;
    if (original != NULL) {
        uint32_t trampoline_size = info.size;
        void *trampoline_base = NULL;
        void *trampoline_from = NULL;
        void *trampoline_to = NULL;

        uint32_t *data = (uint32_t *)target;
        if (info.requires_relocation) {
            for (uint32_t i = 0; i < (info.size/0x4); i++) {
                if ((data[i] & A64_ADR_MASK) == A64_ADRP_OPCODE || (data[i] & A64_ADR_MASK) == A64_ADR_OPCODE) {
                    trampoline_size += 0xc;
                } else if ((data[i] & A64_B_MASK) == A64_B_OPCODE || (data[i] & A64_BL_MASK) == A64_BL_OPCODE) {
                    trampoline_size += 0x10;
                } else if ((data[i] & A64_B_COND_MASK) == A64_B_COND_OPCODE) {
                    trampoline_size += 0x18;
                } else if ((data[i] & A64_CBZ_W_MASK) == A64_CBZ_W_OPCODE || ((data[i] & A64_CBZ_X_MASK) == A64_CBZ_X_OPCODE) || (data[i] & A64_CBNZ_W_MASK) == A64_CBNZ_W_OPCODE || ((data[i] & A64_CBNZ_X_MASK) == A64_CBNZ_X_OPCODE)) {
                    trampoline_size += 0x18;
                }
            }

            trampoline_base = mem_allocate(trampoline_size);
            if (trampoline_base == NULL) return 3;
            uint32_t offset = 0;

            for (uint32_t i = 0; i < (info.size/0x4); i++) {
                if ((data[i] & A64_ADR_MASK) == A64_ADRP_OPCODE || (data[i] & A64_ADR_MASK) == A64_ADR_OPCODE) {
                    install_adr_relocation((void *)&data[i], (void *)((uint64_t)trampoline_base + offset), data[i]);
                    offset += 0x10;
                } else if ((data[i] & A64_B_MASK) == A64_B_OPCODE || (data[i] & A64_BL_MASK) == A64_BL_OPCODE) {
                    install_branch_relocation((void *)&data[i], (void *)((uint64_t)trampoline_base + offset), data[i], scratch_reg);
                    offset += 0x14;
                } else if ((data[i] & A64_B_COND_MASK) == A64_B_COND_OPCODE) {
                    install_branch_relocation((void *)&data[i], (void *)((uint64_t)trampoline_base + offset), data[i], scratch_reg);
                    offset += 0x1c;
                } else if ((data[i] & A64_CBZ_W_MASK) == A64_CBZ_W_OPCODE || ((data[i] & A64_CBZ_X_MASK) == A64_CBZ_X_OPCODE) || (data[i] & A64_CBNZ_W_MASK) == A64_CBNZ_W_OPCODE || ((data[i] & A64_CBNZ_X_MASK) == A64_CBNZ_X_OPCODE)) {
                    install_compare_branch_relocation((void *)&data[i], (void *)((uint64_t)trampoline_base + offset), data[i], scratch_reg);
                    offset += 0x1c;
                } else {
                    *(uint32_t *)((uint64_t)trampoline_base + offset) = data[i];
                    offset += 0x4;
                }
            }

            trampoline_from = (void *)((uint64_t)trampoline_base + trampoline_size);
            trampoline_to = (void *)((uint64_t)target + info.size);
        } else {
            trampoline_size += 0x14;
            trampoline_base = mem_allocate(trampoline_size);
            if (trampoline_base == NULL) return 4;
            mem_copy(trampoline_base, target, info.size);

            trampoline_from = (void *)((uint64_t)trampoline_base + info.size);
            trampoline_to = (void *)((uint64_t)target + info.size);
        }

        install_hook(trampoline_from, trampoline_to, HOOK_TYPE_MOV4_BR, branch_reg);
        if (mem_set_rx(trampoline_base, trampoline_size) != 0) {
            mem_deallocate(trampoline_base, trampoline_size);
            return 5;
        }

        mem_clear_cache(trampoline_base, trampoline_size);
        *original = ptrauth_ia(trampoline_base, NULL);
        orig_base = trampoline_base;
        orig_size = trampoline_size;
    }

    bool installed = false;
    if (mem_set_rw(target, info.size) == 0) {
        install_hook(target, replacement, info.type, branch_reg);
        if (mem_set_rx(target, info.size) == 0) {
            installed = true;
        }
    }

    if (!installed) {
        if (secondary_base != NULL) mem_deallocate(secondary_base, secondary_size);
        if (orig_base != NULL) mem_deallocate(orig_base, orig_size);
        if (original != NULL) *original = NULL;
        return 6;
    }
   
    mem_clear_cache(target, info.size);
    return 0;
}
