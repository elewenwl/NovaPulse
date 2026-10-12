#ifndef basebins_memory_h
#define basebins_memory_h

#include "basebin_common.h"

#define VM_RW (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY)
#define VM_RX (VM_PROT_READ|VM_PROT_EXECUTE)
#define VM_RO (VM_PROT_READ)

extern kern_return_t mach_vm_read_overwrite(mach_port_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
extern kern_return_t mach_vm_allocate(mach_port_t, mach_vm_address_t *, mach_vm_size_t, int);
extern kern_return_t mach_vm_deallocate(mach_port_t, mach_vm_address_t, mach_vm_size_t);
extern kern_return_t mach_vm_write(mach_port_t, mach_vm_address_t, vm_offset_t, mach_msg_type_number_t);
extern kern_return_t mach_vm_protect(mach_port_t, mach_vm_address_t, mach_vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t mach_vm_remap(mach_port_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, mach_port_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);
extern kern_return_t mach_vm_wire(host_priv_t, mach_port_t, mach_vm_address_t, mach_vm_size_t, vm_prot_t);
extern kern_return_t mach_vm_region(mach_port_t, uint64_t *, uint64_t *, uint64_t, void *, uint64_t *, mach_port_t *);
extern kern_return_t mach_vm_copy(mach_port_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t);
extern kern_return_t mach_vm_map(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, mem_entry_name_port_t, memory_object_offset_t, boolean_t, vm_prot_t, vm_prot_t, vm_inherit_t);
extern kern_return_t mach_vm_msync(vm_map_t, mach_vm_address_t, mach_vm_size_t, vm_sync_t);

#if defined(__arm64e__)
void *ptrauth_ia(void *ptr, void *ctx);
void *ptrauth_ib(void *ptr, void *ctx);
void *ptrauth_da(void *ptr, void *ctx);
void *ptrauth_db(void *ptr, void *ctx);
void *autha_xpaci(void *ptr, void *ctx);
void *autha_xpacd(void *ptr, void *ctx);
void *authb_xpaci(void *ptr, void *ctx);
void *authb_xpacd(void *ptr, void *ctx);
void *xpaci(void *ptr);
void *xpacd(void *ptr);
void *hash_discriminator(char *str);
#else
#define ptrauth_ia(ptr, __unused) ((void *)ptr)
#define ptrauth_ib(ptr, __unused) ((void *)ptr)
#define ptrauth_da(ptr, __unused) ((void *)ptr)
#define ptrauth_db(ptr, __unused) ((void *)ptr)
#define autha_xpaci(ptr, __unused) ((void *)ptr)
#define autha_xpacd(ptr, __unused) ((void *)ptr)
#define authb_xpaci(ptr, __unused) ((void *)ptr)
#define authb_xpacd(ptr, __unused) ((void *)ptr)
#define xpaci(ptr) ((void *)ptr)
#define xpacd(ptr) ((void *)ptr)
#define hash_discriminator(__unused) ((void *)0)
#endif

extern mach_port_t mem_task_self;
int mem_set_prot(void *addr, uint32_t size, vm_prot_t prot);
int mem_deallocate(void *addr, uint32_t size);
void mem_copy(void *dest, void *src, uint32_t size);
void mem_clear_cache(void *addr, uint32_t size);
int mem_set_rw(void *addr, uint32_t size);
int mem_set_rx(void *addr, uint32_t size);
void *mem_allocate(uint32_t size);
void *mem_allocate_fixed(void *addr, uint32_t size);

#endif /* basebins_memory_h */
