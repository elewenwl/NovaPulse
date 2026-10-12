#include "info.h"
#include "memory.h"
#include "util.h"
#include "ppl.h"

#define PPL_PAGE_OF(x)              ((x) & ~(kinfo->pmap_page_size-1ULL))
#define PPL_PAGE_OFFSET(x)          ((x) & (kinfo->pmap_page_size-1ULL))

#ifdef __arm64e__
static pthread_mutex_t __ppl_lock = PTHREAD_MUTEX_INITIALIZER;

static void ppl_lock(void) {
    usleep(80);
    pthread_mutex_lock(&__ppl_lock);
    usleep(1000);
}

static void ppl_unlock(void) {
    usleep(80);
    pthread_mutex_unlock(&__ppl_lock);
    usleep(1000);
}

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

void ppl_write_buf(uint64_t va, void *data, uint32_t size) {
    if (!KADDR_VALID(va) || data == NULL || size == 0) return;
    uint64_t pa = kvtophys(va);
    if (pa == 0) return;

    ppl_lock();
    uint64_t target_page = PPL_PAGE_OF(pa);
    uint64_t target_offset = PPL_PAGE_OFFSET(pa);
    
    ppl_set_page(target_page);
    kwrite_buf(kinfo->patches.pplrw_mapping_va + target_offset, data, size);
    ppl_set_page(kinfo->patches.pplrw_mapping_pa);
    ppl_unlock();
}

void ppl_write8(uint64_t va, uint8_t value) {
    ppl_write_buf(va, &value, 1);
}

void ppl_write16(uint64_t va, uint16_t value) {
    ppl_write_buf(va, &value, 2);
}

void ppl_write32(uint64_t va, uint32_t value) {
    ppl_write_buf(va, &value, 4);
}

void ppl_write64(uint64_t va, uint64_t value) {
    ppl_write_buf(va, &value, 8);
}

#endif
