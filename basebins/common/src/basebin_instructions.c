#include "basebin_util.h"
#include "basebin_memory.h"
#include "basebin_instructions.h"

bool a64_is_pc_relative(uint32_t inst, bool ignore_adr, bool ignore_branch) {
    if (!ignore_adr && (inst & A64_ADR_MASK) == A64_ADR_OPCODE) return true;
    if (!ignore_adr && (inst & A64_ADRP_MASK) == A64_ADRP_OPCODE) return true;
    if (!ignore_branch && (inst & A64_B_MASK) == A64_B_OPCODE) return true;
    if (!ignore_branch && (inst & A64_BL_MASK) == A64_BL_OPCODE) return true;
    if (!ignore_branch && (inst & A64_B_COND_MASK) == A64_B_COND_OPCODE) return true;
    if (!ignore_branch && (inst & A64_CBZ_W_MASK) == A64_CBZ_W_OPCODE) return true;
    if (!ignore_branch && (inst & A64_CBZ_X_MASK) == A64_CBZ_X_OPCODE) return true;
    if (!ignore_branch && (inst & A64_CBNZ_W_MASK) == A64_CBNZ_W_OPCODE) return true;
    if (!ignore_branch && (inst & A64_CBNZ_X_MASK) == A64_CBNZ_X_OPCODE) return true;
    if ((inst & A64_TBZ_MASK) == A64_TBZ_OPCODE) return true;
    if ((inst & A64_TBNZ_MASK) == A64_TBNZ_OPCODE) return true;
    if ((inst & A64_LDR_W_LITERAL_MASK) == A64_LDR_W_LITERAL_OPCODE) return true;
    if ((inst & A64_LDR_X_LITERAL_MASK) == A64_LDR_X_LITERAL_OPCODE) return true;
    if ((inst & A64_LDRSW_X_LITERAL_MASK) == A64_LDRSW_X_LITERAL_OPCODE) return true;
    if ((inst & A64_LDR_S_LITERAL_MASK) == A64_LDR_S_LITERAL_OPCODE) return true;
    if ((inst & A64_LDR_D_LITERAL_MASK) == A64_LDR_D_LITERAL_OPCODE) return true;
    if ((inst & A64_LDR_Q_LITERAL_MASK) == A64_LDR_Q_LITERAL_OPCODE) return true;
    if ((inst & A64_PRFM_LITERAL_MASK) == A64_PRFM_LITERAL_OPCODE) return true;
    return false;
}

uint8_t a64_extract_register(uint32_t inst, uint8_t bit) {
    return (inst >> (bit - 4)) & 0x1f;
}

int a64_get_usable_registers(void *target, uint32_t count, uint8_t *branch, uint8_t *scratch) {
    uint32_t *data = (uint32_t *)target;
    uint8_t reg_list[32] = {0};
    reg_list[18] = 1; // apple reserves x18, so it can't be used
    reg_list[8] = 1; // x8 and x9 are used too often to be 100% safe
    reg_list[9] = 1;
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t inst = data[i];
        
        // add/adds (extended register)
        if ((inst & 0x7FE00000) == 0xB200000 || (inst & 0x7FE00000) == 0x2B200000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // add/adds (immediate)
        if ((inst & 0x7F800000) == 0x11000000 || (inst & 0x7F800000) == 0x3100000) {
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // add/adds (shifted register)
        if ((inst & 0x7F200000) == 0xB000000 || (inst & 0x7F200000) == 0x2B000000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // adr/adrp
        if ((inst & A64_ADR_MASK) == A64_ADR_OPCODE || (inst & A64_ADRP_MASK) == A64_ADRP_OPCODE) {
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // and/ands (immediate)
        if ((inst & 0x7F800000) == 0x12000000 || (inst & 0x7F800000) == 0x72000000) {
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // and/ands (shifted register)
        if ((inst & 0x7F200000) == 0xA000000 || (inst & 0x7F200000) == 0x6A000000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // sub/subs (extended register)
        if ((inst & 0x7FE00000) == 0x4B200000 || (inst & 0x7FE00000) == 0x6B200000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // sub/subs (immediate)
        if ((inst & 0x7F800000) == 0x51000000 || (inst & 0x7F800000) == 0x71000000) {
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // sub/subs (shifted register)
        if ((inst & 0x7F200000) == 0x4B000000 || (inst & 0x7F200000) == 0x6B000000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // stp
        if ((inst & 0x7FC00000) == 0x28800000) {
            reg_list[a64_extract_register(inst, 14)] = 1;
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // str (immediate)
        if ((inst & 0xBFE00000) == 0xB8000000) {
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // str/stur (register)
        if ((inst & 0xBFE00C00) == 0xB8200800 || (inst & 0xBFE00C00) == 0xB8000000) {
            reg_list[a64_extract_register(inst, 9)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // mov (bitmask immediate)
        if ((inst & 0x7F8003E0) == 0x320003E0) {
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // mov (register)
        if ((inst & 0x7FE0FFE0) == 0x2A0003E0) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // mov (to/from sp)
        if ((inst & 0x7FFFFC00) == 0x11000000) {
            reg_list[a64_extract_register(inst, 20)] = 1;
            reg_list[a64_extract_register(inst, 4)] = 1;
        }

        // mov (inverted wide immediate), mov (wide immediate), movk/movn/movz
        if ((inst & 0x7F800000) == 0x12800000 || (inst & 0x7F800000) == 0x52800000 || (inst & 0x7F800000) == 0x72800000 || (inst & 0x7F800000) == 0x12800000 || (inst & 0x7F800000) == 0x52800000) {
            reg_list[a64_extract_register(inst, 4)] = 1;
        }
    }

    *branch = 0;
    *scratch = 0;
    if (reg_list[16] == 0) {*branch = 16; reg_list[16] = 1;}
    else if (reg_list[17] == 0) {*branch = 17; reg_list[17] = 1;}
    else if (reg_list[15] == 0) {*branch = 15; reg_list[15] = 1;}
    else if (reg_list[14] == 0) {*branch = 14; reg_list[14] = 1;}
    else if (reg_list[13] == 0) {*branch = 13; reg_list[13] = 1;}
    else if (reg_list[12] == 0) {*branch = 12; reg_list[12] = 1;}
    else if (reg_list[11] == 0) {*branch = 11; reg_list[11] = 1;}
    else if (reg_list[10] == 0) {*branch = 10; reg_list[10] = 1;}

    if (reg_list[16] == 0) *scratch = 16;
    else if (reg_list[17] == 0) *scratch = 17;
    else if (reg_list[15] == 0) *scratch = 15;
    else if (reg_list[14] == 0) *scratch = 14;
    else if (reg_list[13] == 0) *scratch = 13;
    else if (reg_list[12] == 0) *scratch = 12;
    else if (reg_list[11] == 0) *scratch = 11;
    else if (reg_list[10] == 0) *scratch = 10;
    return (*branch != 0 && *scratch != 0) ? 0 : -1;
}


bool a64_is_return(uint32_t inst) {
    if (inst == A64_RET || inst == A64_ERET || inst == A64_BX_LR) return true;
#if defined(__arm64e__)
    if (inst == A64_ERETAA || inst == A64_ERETAB || inst == A64_RETAA || inst == A64_RETAB) return true;
#endif
    return false;
}

// not deterministic
bool a64_is_function_end(uint32_t *data) {
    if (a64_is_return(data[0])) return true;
    if ((data[0] & A64_B_MASK) == A64_B_OPCODE) {
        if (a64_is_return(data[-1]) || (data[-1] & A64_B_MASK) == A64_B_OPCODE) return true;
#if defined(__arm64e__)
         if (data[1] == A64_PACIBSP) return true;
#endif

        if ((data[1] & A64_MOV_X16_MASK) == A64_MOV_X16_OPCODE) {
            if (data[2] == A64_SVC_0x80) return true;
        }

        for (uint32_t i = 0; i < 6; i++) {
            if ((data[i+1] & A64_STP_X29_X30_SP_MASK) == A64_STP_X29_X30_SP_OPCODE) return true;
        }
    }
    return false;
}

uint8_t a64_replaceable_count(void *addr, bool ignore_adr, bool ignore_branch, bool end_only) {
    uint32_t *inst = (uint32_t *)addr;
    uint8_t count = 0;
    
    for (; count < 8; count++) {
        if (!end_only && a64_is_pc_relative(inst[count], ignore_adr, ignore_branch)) break;
        if (a64_is_function_end(&inst[count])) {
            count++;
            break;
        }
    }
    return count;
}

int32_t branch_difference(void *from, void *to) {
    return (from <= to) ? (int32_t)(to - from) : (int32_t)(-(from - to));
}

int32_t page_difference(void *start, void *end) {
    uintptr_t start_page = ARM_PAGE_OF(start);
    uintptr_t end_page = ARM_PAGE_OF(end);

    if (start_page <= end_page) {
        return (int32_t)((end_page - start_page) / ARM_PAGE_SIZE);
    } else {
        return (int32_t)(-((start_page - end_page) / ARM_PAGE_SIZE));
    }
}

bool in_adrp_range(void *start, void *end) {
    uintptr_t min = ARM_PAGE_OF(start) - 0x100000000; // -4GB
    uintptr_t max = ARM_PAGE_OF(start) + 0x100000000; // +4GB
    return (ARM_PAGE_OF(end) <= max && ARM_PAGE_OF(end) >= min);
}

bool in_branch_range(void *start, void *end) {
    uintptr_t min = (uintptr_t)start - 0x8000000; // -128MB
    uintptr_t max = (uintptr_t)start + 0x8000000; // +128MB
    return ((uintptr_t)end <= max && (uintptr_t)end >= min);
}

uint32_t a64_gen_br(uint8_t rd) {
    return (0x20 * rd) | 0xD61F0000;
}

uint32_t a64_gen_blr(uint8_t rd) {
    return (0x20 * rd) | 0xd63f0000;
}

uint32_t a64_gen_b(int32_t imm) {
    return ((imm >> 2) & 0x3FFFFFF) | 0x14000000;
}

uint32_t a64_gen_b_cond(int32_t imm, uint8_t cond) {
    return (((int64_t)imm / 4) << 5) | 0x54000000 | (cond & 0xf);
}

uint32_t a64_gen_movk(uint8_t rd, int32_t imm, uint8_t sh) {
    return rd | 0xF2800000 | (0x20 * imm) | ((int)sh >> 4 << 0x15);
}

uint32_t a64_gen_movz(uint8_t rd, int32_t imm, uint8_t sh) {
    return rd | 0xD2800000 | (0x20 * imm) | ((int)sh >> 4 << 0x15);
}

uint32_t a64_gen_adrp(uint8_t rd, int32_t imm) {
    return ((imm & 3) << 0x1D) | 0x90000000 | (0x20 * ((imm >> 2) & 0x7FFFF)) | rd;
}

uint32_t a64_gen_add(uint8_t rd, uint8_t rn, int32_t imm, uint8_t sh) {
    return (imm << 0xA) | 0x91000000 | ((sh / 0xC) << 0x16) | rd | (0x20 * (uint32_t)rn);
}

uint32_t a64_gen_ldr_literal(uint8_t rd, int32_t imm) {
    return (imm << 5) | 0x58000000 | rd;
}

uint32_t a64_gen_cbz(uint8_t sf, uint8_t rt, int32_t imm) {
    return 0x34000000 | ((sf & 0x1) << 31) | (((uint32_t)imm >> 2) << 5) | rt;
}

uint32_t a64_gen_cbnz(uint8_t sf, uint8_t rt, int32_t imm) {
    return 0x35000000 | ((sf & 0x1) << 31) | (((uint32_t)imm >> 2) << 5) | rt;
}
