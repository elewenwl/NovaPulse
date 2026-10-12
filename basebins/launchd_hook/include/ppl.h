#ifndef launchd_hook_ppl_h
#define launchd_hook_ppl_h

#include "basebin_common.h"

#ifdef __arm64e__
void ppl_write_buf(uint64_t va, void *data, uint32_t size);
void ppl_write8(uint64_t va, uint8_t val);
void ppl_write16(uint64_t va, uint16_t val);
void ppl_write32(uint64_t va, uint32_t val);
void ppl_write64(uint64_t va, uint64_t val);
#else
#define ppl_write_buf(va, data, size);
#define ppl_write8(va, value);
#define ppl_write16(va, value);
#define ppl_write32(va, value);
#define ppl_write64(va, value);
#endif

#endif /* launchd_hook_ppl_h */
