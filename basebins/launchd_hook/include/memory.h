#ifndef launchd_hook_memory_h
#define launchd_hook_memory_h

#include "info.h"
#include "basebin_memory.h"

#define ARM_TTE_VALID               0x0000000000000001ull
#define ARM_TTE_TYPE_MASK           0x0000000000000002ull
#define ARM_TTE_TYPE_TABLE          0x0000000000000002ull
#define ARM_TTE_TYPE_BLOCK          0x0000000000000000ull
#define ARM_TTE_TABLE_MASK          0x0000fffffffff000ULL
#define ARM_TTE_PA_MASK             0x0000fffffffff000ull
#define ARM_TTE_TYPE_L3BLOCK        0x0000000000000002ull
#define PMAP_TT_L0_LEVEL            0x0
#define PMAP_TT_L1_LEVEL            0x1
#define PMAP_TT_L2_LEVEL            0x2
#define PMAP_TT_L3_LEVEL            0x3

#define ARM_16K_TT_L0_SIZE          0x0000800000000000ull
#define ARM_16K_TT_L0_OFFMASK       0x00007fffffffffffull
#define ARM_16K_TT_L0_SHIFT         47
#define ARM_16K_TT_L0_INDEX_MASK    0x0000800000000000ull
#define ARM_16K_TT_L1_SIZE          0x0000001000000000ull
#define ARM_16K_TT_L1_OFFMASK       0x0000000fffffffffull
#define ARM_16K_TT_L1_SHIFT         36
#define ARM_16K_TT_L1_INDEX_MASK    0x0000007000000000ull
#define ARM_16K_TT_L2_SIZE          0x0000000002000000ull
#define ARM_16K_TT_L2_OFFMASK       0x0000000001ffffffull
#define ARM_16K_TT_L2_SHIFT         25
#define ARM_16K_TT_L2_INDEX_MASK    0x0000000ffe000000ull
#define ARM_16K_TT_L3_SIZE          0x0000000000004000ull
#define ARM_16K_TT_L3_OFFMASK       0x0000000000003fffull
#define ARM_16K_TT_L3_SHIFT         14
#define ARM_16K_TT_L3_INDEX_MASK    0x0000000001ffc000ull

typedef struct {
    uint64_t off_mask;
    uint64_t shift;
    uint64_t index_mask;
    uint64_t valid_mask;
    uint64_t type_mask;
    uint64_t type_block;
} tt_level_t;

typedef struct {
    uint64_t pa;
    uint64_t va;
    uint64_t len;
} ptov_table_t;

typedef struct {
    uint32_t wait_for_space:1;
    uint32_t wiring_required:1;
    uint32_t no_zero_fill:1;
    uint32_t mapped_in_other_pmaps:1;
    uint32_t switch_protect:1;
    uint32_t disable_vmentry_reuse:1;
    uint32_t map_disallow_data_exec:1;
    uint32_t holelistenabled:1;
    uint32_t is_nested_map:1;
    uint32_t map_disallow_new_exec:1;
    uint32_t jit_entry_exists:1;
    uint32_t has_corpse_footprint:1;
    uint32_t warned_delete_gap:1;
    uint32_t pad:19;
} vm_map_flags_t;

void kread_buf(uint64_t addr, void *data, uint32_t size);
void kwrite_buf(uint64_t addr, void *data, uint32_t size);
uint8_t kread8(uint64_t addr);
uint16_t kread16(uint64_t addr);
uint32_t kread32(uint64_t addr);
uint64_t kread64(uint64_t addr);
void kwrite8(uint64_t addr, uint8_t value);
void kwrite16(uint64_t addr, uint16_t value);
void kwrite32(uint64_t addr, uint32_t value);
void kwrite64(uint64_t addr, uint64_t value);  
uint64_t kalloc(uint32_t size);
uint64_t kalloc_wired(uint32_t size);
void kfree(uint64_t addr, uint32_t size);
void kzero(uint64_t addr, uint32_t size);
uint64_t phystokv(uint64_t pa);
uint64_t get_va_pte(uint64_t va);
uint64_t kvtophys(uint64_t va);
uint64_t vtophys(uint64_t va);

#endif /* launchd_hook_memory_h */
