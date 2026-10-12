#include "info.h"
#include "util.h"
#include "ppl.h"
#include "basebin_memory.h"
#include "memory.h"

static tt_level_t arm_tt_level[4] = {
    (tt_level_t){
        .off_mask = ARM_16K_TT_L0_OFFMASK,
        .shift = ARM_16K_TT_L0_SHIFT,
        .index_mask = ARM_16K_TT_L0_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L1_OFFMASK,
        .shift = ARM_16K_TT_L1_SHIFT,
        .index_mask = ARM_16K_TT_L1_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L2_OFFMASK,
        .shift = ARM_16K_TT_L2_SHIFT,
        .index_mask = ARM_16K_TT_L2_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L3_OFFMASK,
        .shift = ARM_16K_TT_L3_SHIFT,
        .index_mask = ARM_16K_TT_L3_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_L3BLOCK
    }
};

void kread_buf(uint64_t addr, void *data, uint32_t size) {
    if (!KADDR_VALID(addr) || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_vm_size_t read_size = 0;
        mach_vm_size_t current_size = 1024;
        if (current_size > (size - offset)) current_size = size - offset;
        
        int kr = mach_vm_read_overwrite(kinfo->tfp0, addr + offset, current_size, (uint64_t)data + offset, &read_size);
        if (kr != 0 || read_size != current_size) break;
        offset += read_size;
    }
}

void kwrite_buf(uint64_t addr, void *data, uint32_t size) {
    if (!KADDR_VALID(addr) || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_msg_type_number_t current_size = 1024;
        if (current_size > (size - offset)) current_size = size - offset;
        
        if (mach_vm_write(kinfo->tfp0, addr + offset, (uint64_t)data + offset, current_size) != 0) break;
        offset += current_size;
    }
}

uint8_t kread8(uint64_t addr) {
    uint8_t value = 0;
    kread_buf(addr, &value, 1);
    return value;
}

uint16_t kread16(uint64_t addr) {
    uint16_t value = 0;
    kread_buf(addr, &value, 2);
    return value;
}

uint32_t kread32(uint64_t addr) {
    uint32_t value = 0;
    kread_buf(addr, &value, 4);
    return value;
}

uint64_t kread64(uint64_t addr) {
    uint64_t value = 0;
    kread_buf(addr, &value, 8);
    return value;
}

void kwrite8(uint64_t addr, uint8_t value) {
    kwrite_buf(addr, &value, 1);
}

void kwrite16(uint64_t addr, uint16_t value) {
    kwrite_buf(addr, &value, 2);
}

void kwrite32(uint64_t addr, uint32_t value) {
    kwrite_buf(addr, &value, 4);
}

void kwrite64(uint64_t addr, uint64_t value) {
    kwrite_buf(addr, &value, 8);
}

uint64_t kalloc(uint32_t size) {
    if (size == 0) return 0;
    uint64_t addr = 0;
    mach_vm_allocate(kinfo->tfp0, &addr, size, VM_FLAGS_ANYWHERE);
    return addr;
}

uint64_t kalloc_wired(uint32_t size) {
    if (size == 0) return 0;
    size = (uint32_t)round_page_kernel(size);
    
    uint64_t addr = 0;
    mach_vm_allocate(kinfo->tfp0, &addr, size, VM_FLAGS_ANYWHERE|VM_FLAGS_NO_CACHE);
    if (!KADDR_VALID(addr)) return 0;
    
    if (mach_vm_wire(mach_host_self(), kinfo->tfp0, addr, size, VM_PROT_READ|VM_PROT_WRITE) != 0) {
        kzero(addr, size);
    }
    return addr;
}

void kfree(uint64_t addr, uint32_t size) {
    if (!KADDR_VALID(addr) || size == 0) return;
    mach_vm_deallocate(kinfo->tfp0, addr, size);
}

void kzero(uint64_t addr, uint32_t size) {
    if (!KADDR_VALID(addr) || size == 0) return;
    void *data = calloc(1, size);
    if (data == NULL) return;

    bzero(data, size);
    kwrite_buf(addr, data, size);
    free(data);
}

uint64_t phystokv(uint64_t pa) {
    if (kinfo->patches.ptov_table == 0 || pa < kinfo->patches.gPhysBase) return 0;
    ptov_table_t table[8] = {};
    
    kread_buf(kinfo->patches.ptov_table, &table[0], sizeof(table));
    for (uint64_t i = 0; (i < 8) && (table[i].len != 0); i++) {
        if ((pa >= table[i].pa) && (pa < (table[i].pa + table[i].len))) {
            return pa - table[i].pa + table[i].va;
        }
    }
    return pa - kinfo->patches.gPhysBase + kinfo->patches.gVirtBase;
}

static uint64_t ttep_vtophys(uint64_t ttep, uint64_t va) {
    if (va == 0 || ttep == 0) return 0;
    for (uint64_t level = PMAP_TT_L1_LEVEL; level <= PMAP_TT_L3_LEVEL; level++) {
        if (level > PMAP_TT_L3_LEVEL || ttep == 0) return 0;
        tt_level_t *level_info = &arm_tt_level[level];

        uint64_t tte_index = (va & level_info->index_mask) >> level_info->shift;
        uint64_t tte_pa = ttep + (tte_index * 0x8);
        uint64_t tte_va = phystokv(tte_pa);
        if (!KADDR_VALID(tte_va)) return 0;

        uint64_t tte_entry = kread64(tte_va);
        if ((tte_entry & level_info->valid_mask) != level_info->valid_mask) return 0;
        if ((tte_entry & level_info->type_mask) == level_info->type_block) {
            return ((tte_entry & ARM_TTE_PA_MASK & ~level_info->off_mask) | (va & level_info->off_mask));
        }
        ttep = tte_entry & ARM_TTE_TABLE_MASK;
    }
    return ttep;
}

uint64_t get_va_pte(uint64_t va) {
    uint64_t ttep = kinfo->self_ttep;
    uint64_t tte_pa = 0;

    for (uint64_t level = PMAP_TT_L1_LEVEL; level <= PMAP_TT_L3_LEVEL; level++) {
        if (level > PMAP_TT_L3_LEVEL || ttep == 0) return 0;
        tt_level_t *level_info = &arm_tt_level[level];

        uint64_t tte_index = (va & level_info->index_mask) >> level_info->shift;
        tte_pa = ttep + (tte_index * 0x8);
        uint64_t tte_va = phystokv(tte_pa);
        if (tte_va == 0) return 0;

        uint64_t tte_entry = kread64(tte_va);
        if ((tte_entry & level_info->valid_mask) != level_info->valid_mask) return 0;
        if ((tte_entry & level_info->type_mask) == level_info->type_block) {
            return tte_pa;
        }
        ttep = tte_entry & ARM_TTE_TABLE_MASK;
    }
    return tte_pa;
}

uint64_t kvtophys(uint64_t va) {
    if (kinfo->cpu_ttep == 0 || !KADDR_VALID(va) || va < kinfo->patches.gVirtBase) return 0;
    return ttep_vtophys(kinfo->cpu_ttep, va);
}

uint64_t vtophys(uint64_t va) {
    if (kinfo->self_ttep == 0 || va == 0) return 0;
    return ttep_vtophys(kinfo->self_ttep, va);
}
