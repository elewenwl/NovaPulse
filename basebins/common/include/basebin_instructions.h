#ifndef basebin_instructions_h
#define basebin_instructions_h

#include "basebin_common.h"

#define ARM_PAGE_SIZE               0x1000
#define ARM_PAGE_MASK               (~(ARM_PAGE_SIZE-1))

#define ARM_PAGE_OF(addr)           ((uintptr_t)addr & ARM_PAGE_MASK)
#define ARM_PAGE_OFFSET(addr)       ((uint32_t)((uintptr_t)addr & ~ARM_PAGE_MASK))
#define SIZE_ALIGN(size)            (((uint32_t)(size) + (4 - 1)) & ~(4 - 1))
#define VM_PAGE_OF(addr)            ((uintptr_t)addr & ~((uintptr_t)(vm_page_size - 1)))

#define A64_ADR_MASK                0x9f000000
#define A64_ADR_OPCODE              0x10000000
#define A64_ADRP_MASK               0x9f000000
#define A64_ADRP_OPCODE             0x90000000
#define A64_B_MASK                  0xfc000000
#define A64_B_OPCODE                0x14000000
#define A64_BL_MASK                 0xfc000000
#define A64_BL_OPCODE               0x94000000
#define A64_B_COND_MASK             0xff000010
#define A64_B_COND_OPCODE           0x54000000
#define A64_CBZ_W_MASK              0xff000000
#define A64_CBZ_W_OPCODE            0x34000000
#define A64_CBZ_X_MASK              0xff000000
#define A64_CBZ_X_OPCODE            0xb4000000
#define A64_CBNZ_W_MASK             0xff000000
#define A64_CBNZ_W_OPCODE           0x35000000
#define A64_CBNZ_X_MASK             0xff000000
#define A64_CBNZ_X_OPCODE           0xb5000000
#define A64_TBZ_MASK                0x7f000000
#define A64_TBZ_OPCODE              0x36000000
#define A64_TBNZ_MASK               0x7f000000
#define A64_TBNZ_OPCODE             0x37000000
#define A64_LDR_W_LITERAL_MASK      0xff000000
#define A64_LDR_W_LITERAL_OPCODE    0x18000000
#define A64_LDR_X_LITERAL_MASK      0xff000000
#define A64_LDR_X_LITERAL_OPCODE    0x58000000
#define A64_LDRSW_X_LITERAL_MASK    0xff000000
#define A64_LDRSW_X_LITERAL_OPCODE  0x98000000
#define A64_LDR_S_LITERAL_MASK      0xff000000
#define A64_LDR_S_LITERAL_OPCODE    0x1c000000
#define A64_LDR_D_LITERAL_MASK      0xff000000
#define A64_LDR_D_LITERAL_OPCODE    0x5c000000
#define A64_LDR_Q_LITERAL_MASK      0xff000000
#define A64_LDR_Q_LITERAL_OPCODE    0x9c000000
#define A64_PRFM_LITERAL_MASK       0xff000000
#define A64_PRFM_LITERAL_OPCODE     0xd8000000

#define A64_RET                     0xd65f03c0
#define A64_ERET                    0xd69f03e0
#define A64_ERETAA                  0xd69f0bff /* FEAT_PAuth */
#define A64_ERETAB                  0xd69f0fff /* FEAT_PAuth */
#define A64_RETAA                   0xd65f0bff /* FEAT_PAuth */
#define A64_RETAB                   0xd65f0fff /* FEAT_PAuth */
#define A64_PACIBSP                 0xd503237f /* FEAT_PAuth */
#define A64_BX_LR                   0xd61f03c0
#define A64_NOP                     0xd503201f
#define A64_SVC_0x80                0xd4001001
#define A64_BR_X16                  0xd61f0200
#define A64_BLR_X16                 0xd63f0200
#define A64_LDR_X16_0x8             0x58000050
#define A64_LDR_X0_X8_0xC8          0xf9406500
#define A64_LDR_X8_X0               0xf9400008
#define A64_ADD_MASK                0xff000000
#define A64_ADD_OPCODE              0x91000000

#define A64_MOV_X16_MASK            0xffe0001f
#define A64_MOV_X16_OPCODE          0xd2800010
#define A64_STP_X29_X30_SP_MASK     0xff00ffff
#define A64_STP_X29_X30_SP_OPCODE   0xa9007bfd

bool a64_is_pc_relative(uint32_t inst, bool ignore_adr, bool ignore_branch);
uint8_t a64_replaceable_count(void *addr, bool ignore_adr, bool ignore_branch, bool end_only);

int32_t branch_difference(void *from, void *to);
int32_t page_difference(void *start, void *end);
bool in_adrp_range(void *start, void *end);
bool in_branch_range(void *start, void *end);
uint8_t a64_extract_register(uint32_t inst, uint8_t bit);
int a64_get_usable_registers(void *target, uint32_t count, uint8_t *branch, uint8_t *scratch);

uint32_t a64_gen_br(uint8_t rd);
uint32_t a64_gen_blr(uint8_t rd);
uint32_t a64_gen_b(int32_t imm);
uint32_t a64_gen_b_cond(int32_t imm, uint8_t cond);
uint32_t a64_gen_bc_cond(int32_t imm, uint8_t cond);
uint32_t a64_gen_movk(uint8_t rd, int32_t imm, uint8_t sh);
uint32_t a64_gen_movz(uint8_t rd, int32_t imm, uint8_t sh);
uint32_t a64_gen_adrp(uint8_t rd, int32_t imm);
uint32_t a64_gen_add(uint8_t rd, uint8_t rn, int32_t imm, uint8_t sh);
uint32_t a64_gen_ldr_literal(uint8_t rd, int32_t imm);
uint32_t a64_gen_cbz(uint8_t sf, uint8_t rt, int32_t imm);
uint32_t a64_gen_cbnz(uint8_t sf, uint8_t rt, int32_t imm);
uint32_t a64_gen_brk(int32_t imm);

#endif /* basebin_instructions_h */
