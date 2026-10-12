#ifndef jbutil_process_h
#define jbutil_process_h

#include "common.h"

#define CS_HARD                     0x00000100
#define CS_KILL                     0x00000200
#define CS_OPS_STATUS               0x00000000
#define FLAG_PLATFORMIZE            (1 << 1)
#define PROC_ALL_PIDS               1
#define PROC_PIDPATHINFO            11
#define PROC_PIDPATHINFO_SIZE       (MAXPATHLEN)
#define PROC_PIDPATHINFO_MAXSIZE    (4*MAXPATHLEN)

#define DSC_ARM64 "/System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64"
#define DSC_ARM64E "/System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64e"

typedef enum {
    PROCESS_ARCH_UNKNOWN = -1,
    PROCESS_ARCH_ARMV6,
    PROCESS_ARCH_ARMV7,
    PROCESS_ARCH_ARMV7S,
    PROCESS_ARCH_ARM64,
    PROCESS_ARCH_ARM64E
} process_arch_t;

typedef struct {
    pid_t pid;
    mach_port_t task;
    process_arch_t arch;
    bool is_64bit;
    uint64_t image_base;
} process_ctx_t;

typedef __darwin_natural_t natural_t;
typedef int integer_t;
typedef uintptr_t vm_offset_t;
typedef uintptr_t vm_size_t;
typedef uint64_t mach_vm_address_t;
typedef uint64_t mach_vm_offset_t;
typedef uint64_t mach_vm_size_t;
typedef uint64_t vm_map_offset_t;
typedef uint64_t vm_map_address_t;
typedef uint64_t vm_map_size_t;

extern kern_return_t mach_vm_read_overwrite(mach_port_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
extern kern_return_t mach_vm_allocate(mach_port_t, mach_vm_address_t *, mach_vm_size_t, int);
extern kern_return_t mach_vm_deallocate(mach_port_t, mach_vm_address_t, mach_vm_size_t);
extern kern_return_t mach_vm_write(mach_port_t, mach_vm_address_t, vm_offset_t, mach_msg_type_number_t);
extern kern_return_t mach_vm_protect(mach_port_t, mach_vm_address_t, mach_vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t mach_vm_remap(mach_port_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, mach_port_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);
extern kern_return_t mach_vm_wire(host_priv_t, mach_port_t, mach_vm_address_t, mach_vm_size_t, vm_prot_t);
extern int csops(pid_t pid, unsigned int  ops, void * useraddr, size_t usersize);
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int proc_pidpath(int pid, void * buffer, uint32_t  buffersize);
extern int proc_name(int pid, void * buffer, uint32_t buffersize);

uint64_t process_alloc(process_ctx_t *ctx, uint32_t size);
int process_dealloc(process_ctx_t *ctx, uint64_t addr, size_t size);
int process_protect(process_ctx_t *ctx, uint64_t addr, uint32_t size, vm_prot_t prot);
int process_read(process_ctx_t *ctx, uint64_t addr, void *buf, uint32_t size);
int process_write(process_ctx_t *ctx, uint64_t addr, void *buf, uint32_t size);
uint64_t process_read64(process_ctx_t *ctx, uint64_t addr);
uint32_t process_read32(process_ctx_t *ctx, uint64_t addr);
void process_write64(process_ctx_t *ctx, uint64_t addr, uint64_t value);
void process_write32(process_ctx_t *ctx, uint64_t addr, uint32_t value);
uintptr_t process_create_str(process_ctx_t *ctx, char *str);
pid_t process_get_pid(const char *name);
int process_send_signal(pid_t pid, int num);
int process_get_root(void);
int process_run(const char *path, char **argv, bool use_root);
process_ctx_t *process_init(pid_t pid);
void process_release(process_ctx_t *ctx);
mach_port_t process_get_task(pid_t pid);
uint64_t process_find_image(process_ctx_t *ctx, const char *path);
uint64_t process_find_symbol(process_ctx_t *ctx, uintptr_t image, const char *name);
uint64_t process_find_private_symbol(process_ctx_t *ctx, const char *path, const char *name);
uint64_t process_find_spin_gadget(process_ctx_t *ctx);

#endif /* jbutil_process_h */
