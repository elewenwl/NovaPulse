#ifndef launchd_hook_utils_h
#define launchd_hook_utils_h

#include "info.h"
#include "basebin_util.h"
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>

#define CFNUM(val) CFNumberCreate(NULL, kCFNumberIntType, &(int){val})
#define KADDR_VALID(addr) (((addr) & 0xffff000000000000) == 0xffff000000000000)
#define SANDBOX_SLOT 1

typedef enum {
    SOC_UNKNOWN = -1,
    SOC_A7,
    SOC_A8,
    SOC_A8X,
    SOC_A9,
    SOC_A9X,
    SOC_A10,
    SOC_A10X,
    SOC_A11,
    SOC_A12,
    SOC_A12X,
    SOC_A12Z,
    SOC_A13
} soc_type_t;

extern int IOMobileFramebufferGetMainDisplay(void**);
extern int IOMobileFramebufferGetLayerDefaultSurface(void*, int, void **);
extern int IOMobileFramebufferGetDisplaySize(void *pointer, CGSize *size);
extern void *IOSurfaceCreate(CFDictionaryRef properties);
extern int IOSurfaceLock(void *buffer, uint32_t options, uint32_t *seed);
extern int IOSurfaceUnlock(void *buffer, uint32_t options, uint32_t *seed);
extern int IOMobileFramebufferSwapBegin(void *, int *);
extern int IOMobileFramebufferSwapSetLayer(void *, int, void *, CGRect, CGRect, int);
extern int IOMobileFramebufferSwapEnd(void *);
extern void *IOSurfaceGetBaseAddress(void *buffer);
extern size_t IOSurfaceGetBytesPerRow(void *buffer);
extern size_t IOSurfaceAlignProperty(CFStringRef property, size_t value);
extern const CFStringRef kIOSurfaceBytesPerRow;

int get_process_info(pid_t pid, uint64_t *proc_addr, uint64_t *task_addr);
uint64_t find_proc_for_pid(pid_t pid);
uint64_t find_task_for_pid(pid_t pid);
pid_t find_pid_for_name(const char *name);
uint64_t find_ipc_port(mach_port_t port);
void set_mac_slot(pid_t pid, uint64_t proc, int slot, uint64_t value);
void add_task_flag(pid_t pid, uint64_t task, int flag);
void add_cs_flag(pid_t pid, uint64_t proc, int flag);
void remove_cs_flag(pid_t pid, uint64_t proc, int flag);
uint64_t vnode_for_fd(int fd);
uint64_t proc_get_pmap_cs_entry(uint64_t proc);
soc_type_t get_soc_type(void);
int draw_splash_screen(void);
int update_jailbreak(void);

#endif /* launchd_hook_utils_h */
