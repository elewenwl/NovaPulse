#ifndef patchfinder_h
#define patchfinder_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <CoreFoundation/CoreFoundation.h>

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
#define A64_ADD_MASK                0xff000000
#define A64_ADD_OPCODE              0x91000000
#define A64_LDR_X_LITERAL_MASK      0xff000000
#define A64_LDR_X_LITERAL_OPCODE    0x58000000
#define A64_ALL_MASK                0xffffffff
#define A64_RETAA                   0xd65f0bff /* FEAT_PAuth */
#define A64_RETAB                   0xd65f0fff /* FEAT_PAuth */
#define A64_RET                     0xd65f03c0

#define A64_IS_BRANCH(inst) (((inst & A64_B_MASK) == A64_B_OPCODE) || \
    ((inst & A64_BL_MASK) == A64_BL_OPCODE) || \
    ((inst & A64_B_COND_MASK) == A64_B_COND_OPCODE))

#define A64_IS_COMPARE_BRANCH(inst) (((inst & A64_CBZ_W_MASK) == A64_CBZ_W_OPCODE) || \
    ((inst & A64_CBZ_X_MASK) == A64_CBZ_X_OPCODE) || \
    ((inst & A64_CBNZ_W_MASK) == A64_CBNZ_W_OPCODE) || \
    ((inst & A64_CBNZ_X_MASK) == A64_CBNZ_X_OPCODE))

typedef struct {
    uint64_t base;
    uint64_t size;
} kernel_section_t;

typedef struct {
    uint8_t *data;
    uint32_t *size;
    uint64_t search_base;
    uint32_t search_size;
    uint64_t prelink_offset;
    uint32_t prelink_size;
    uint64_t kernel_entry;
    struct mach_header_64 *hdr;
    
    struct {
        kernel_section_t text;
        kernel_section_t cstring;
        kernel_section_t os_log;
        kernel_section_t bss;
        kernel_section_t ppl_text;
        kernel_section_t prelink_text;
        kernel_section_t prelink_string;
    } section;
} kpf_ctx_t;

void kpf_deinit(void);
int kpf_init(void);
uint64_t kpf_find_dynamic_trustcache(void);
uint64_t kpf_find_static_trustcache(void);
uint64_t kpf_find_ptov_table(void);
uint64_t kpf_find_pidhash_ptr(void);
uint64_t kpf_find_pidhashtbl_ptr(void);
uint64_t kpf_find_all_proc(void);
uint64_t kpf_find_cpu_ttep_ptr(void);
uint64_t kpf_find_cpu_ttep(void);
uint64_t kpf_find_self_ttep(void);
uint64_t kpf_find_pv_head_table_ptr(void);
uint64_t kpf_find_pv_head_table(void);
uint64_t kpf_find_pe_state(void);
uint64_t kpf_find_bootargs(void);
uint64_t kpf_find_gfx_mapping_base(void);

#endif /* patchfinder_h */
