#include "injector.h"
#include "process.h"
#include "util.h"
#include "dyld.h"
#include "tnsv2.h"

static uint64_t kernel_base = 0;
static uint64_t kernel_slide = 0;
static uint64_t kern_proc_addr = 0;
static uint64_t kern_task_addr = 0;
static uint64_t launchd_proc_addr = 0;
static uint64_t launchd_task_addr = 0;
static uint64_t self_proc_addr = 0;
static uint64_t self_task_addr = 0;
static uint64_t all_proc = 0;
static uint64_t static_trustcache = 0;
static uint64_t dynamic_trustcache = 0;
static uint64_t pidhash_ptr = 0;
static uint64_t pidhashtbl_ptr = 0;
static mach_port_t tfp0 = MACH_PORT_NULL;

static void kread_buf(uint64_t addr, void *data, uint32_t size) {
    if (addr == 0 || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_vm_size_t read_size = 0;
        mach_vm_size_t current_size = 1024;
        if (current_size > (size - offset)) current_size = size - offset;
        
        int kr = mach_vm_read_overwrite(tfp0, addr + offset, current_size, (uint64_t)data + offset, &read_size);
        if (kr != 0 || read_size != current_size) break;
        offset += read_size;
    }
}

static void kwrite_buf(uint64_t addr, void *data, uint32_t size) {
    if (addr == 0 || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_msg_type_number_t current_size = 1024;
        if (current_size > (size - offset)) current_size = size - offset;
        
        if (mach_vm_write(tfp0, addr + offset, (uint64_t)data + offset, current_size) != 0) break;
        offset += current_size;
    }
}

static uint8_t kread8(uint64_t addr) {
    uint8_t value = 0;
    kread_buf(addr, &value, 1);
    return value;
}

static uint16_t kread16(uint64_t addr) {
    uint16_t value = 0;
    kread_buf(addr, &value, 2);
    return value;
}

static uint32_t kread32(uint64_t addr) {
    uint32_t value = 0;
    kread_buf(addr, &value, 4);
    return value;
}

static uint64_t kread64(uint64_t addr) {
    uint64_t value = 0;
    kread_buf(addr, &value, 8);
    return value;
}

static void kwrite8(uint64_t addr, uint8_t value) {
    kwrite_buf(addr, &value, 1);
}

static void kwrite16(uint64_t addr, uint16_t value) {
    kwrite_buf(addr, &value, 2);
}

static void kwrite32(uint64_t addr, uint32_t value) {
    kwrite_buf(addr, &value, 4);
}

static void kwrite64(uint64_t addr, uint64_t value) {
    kwrite_buf(addr, &value, 8);
}

static xpc_object_t xpc_open_plist(const char *path) {
    uint32_t file_size = 0;
    uint8_t *file_data = map_file(path, &file_size, false);
    if (file_data == NULL) return NULL;

    xpc_object_t plist = xpc_create_from_plist(file_data, file_size);
    unmap_file(file_data, file_size);

    if (plist == NULL || xpc_get_type(plist) != XPC_TYPE_DICTIONARY) {
        xpc_release(plist);
        return NULL;
    }
    return plist;
}

static uint64_t dict_get_u64(xpc_object_t dict, const char *key) {
    xpc_object_t value = xpc_dictionary_get_value(dict, key);
    if (value == NULL) return 0;

    if (xpc_get_type(value) == XPC_TYPE_STRING) {
        const char *str = xpc_string_get_string_ptr(value);
        if (str == NULL) return 0;

        if (strlen(str) >= 3 && str[0] == '0' && str[1] == 'x') {
            return (uint64_t)strtoull(str, (char **)&str, 16);
        } else {
            return (uint64_t)strtoull(str, (char **)&str, 10);
        }
    } else if (xpc_get_type(value) == XPC_TYPE_UINT64) {
        return xpc_uint64_get_value(value);
    }
    return 0;
}

static void add_plist_entry(CFMutableDictionaryRef plist, CFStringRef key, uint64_t value) {
    CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("0x%llx"), value);
    CFDictionaryAddValue(plist, key, value_str);
}

static int init_handoff_plist(void) {
    CFMutableDictionaryRef plist = NULL;
    CFWriteStreamRef stream = NULL;
    CFURLRef url = NULL;
    CFDataRef data = NULL;
    int status = -1;
    
    if ((plist = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL)) == NULL) goto done;
    unlink("/amethyst/handoff.plist");

    add_plist_entry(plist, CFSTR("kern_proc_addr"), kern_proc_addr);
    add_plist_entry(plist, CFSTR("kern_task_addr"), kern_task_addr);
    add_plist_entry(plist, CFSTR("kernel_base"), kernel_base);
    add_plist_entry(plist, CFSTR("kernel_slide"), kernel_slide);
    add_plist_entry(plist, CFSTR("static_trustcache"), static_trustcache);
    add_plist_entry(plist, CFSTR("dynamic_trustcache"), dynamic_trustcache);
    if (pidhash_ptr != 0) add_plist_entry(plist, CFSTR("pidhash_ptr"), pidhash_ptr);
    if (pidhashtbl_ptr != 0) add_plist_entry(plist, CFSTR("pidhashtbl_ptr"), pidhashtbl_ptr);
    add_plist_entry(plist, CFSTR("tnsv2"), 0x4141);

    if ((url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/amethyst/handoff.plist"), 0, 0)) == NULL) goto done;
    if ((data = CFPropertyListCreateData(kCFAllocatorDefault, plist, kCFPropertyListXMLFormat_v1_0, 0, NULL)) == NULL) goto done;
    if ((stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, url)) == NULL) goto done;
    if (!CFWriteStreamOpen(stream)) goto done;
    
    CFWriteStreamWrite(stream, CFDataGetBytePtr(data), CFDataGetLength(data));
    CFWriteStreamClose(stream);
    status = 0;
    sync();
    
done:
    if (plist != NULL) CFRelease(plist);
    if (stream != NULL) CFRelease(stream);
    if (url != NULL) CFRelease(url);
    if (data != NULL) CFRelease(data);
    return status;
}

static uint64_t find_proc_for_pid(pid_t pid) {
    uint64_t current_proc = kread64(all_proc);
    while (true) {
        if (current_proc == 0) return 0;
        pid_t current_pid = kread32(current_proc + 0x60);
        if (current_pid == pid) return current_proc;
        current_proc = kread64(current_proc);
    }
    return true;
}

static uint64_t vnode_for_fd(int fd) {
    if (fd < 0) return 0;
    uint64_t p_fd = kread64(self_proc_addr + 0x100);
    if (p_fd == 0) return 0;
    
    uint64_t fd_ofiles = kread64(p_fd + 0x0);
    if (fd_ofiles == 0) return 0;

    uint64_t fproc = kread64(fd_ofiles + (fd * 8));
    if (fproc == 0) return 0;
    
    uint64_t f_fglob = kread64(fproc + 0x8);
    if (f_fglob == 0) return 0;
    return kread64(f_fglob + 0x38);
}

static int swap_namecache_vnode(const char *src, const char *dest) {
    int src_fd = -1;
    int dest_fd = -1;
    int status = -1;
    
    if ((src_fd = open(src, O_RDONLY)) < 0) goto done;
    if ((dest_fd = open(dest, O_RDONLY)) < 0) goto done;
    usleep(10000);
    sync();
    
    uint64_t src_vnode = vnode_for_fd(src_fd);
    if (src_vnode == 0) goto done;
    
    uint64_t dest_vnode = vnode_for_fd(dest_fd);
    if (dest_vnode == 0) goto done;

    uint64_t namecache = kread64(dest_vnode + 0x40);
    if (namecache == 0) goto done;
    
    kwrite32(src_vnode + 0x5c, kread32(src_vnode + 0x5c) + 10);
    kwrite32(src_vnode + 0x60, kread32(src_vnode + 0x60) + 10);
    kwrite32(dest_vnode + 0x5c, kread32(dest_vnode + 0x5c) + 10);
    kwrite32(dest_vnode + 0x60, kread32(dest_vnode + 0x60) + 10);
    kwrite64(namecache + 0x48, src_vnode);
    
    status = 0;
    usleep(10000);
    sync();
    
done:
    if (src_fd >= 0) close(src_fd);
    if (dest_fd >= 0) close(dest_fd);
    return status;
}

int vnode_hide_path(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    usleep(10000);
    sync();
    
    uint64_t vnode = vnode_for_fd(fd);
    if (vnode == 0) {
        close(fd);
        return -1;
    }
    
    kwrite32(vnode + 0x5c, kread32(vnode + 0x5c) + 10);
    kwrite32(vnode + 0x60, kread32(vnode + 0x60) + 10);
    kwrite32(vnode + 0x54, kread32(vnode + 0x54) | VISSHADOW);
    usleep(10000);
    sync();
    
    close(fd);
    return 0;
}

int tnsv2_startup(void) {
    xpc_object_t chimera_plist = NULL;
    xpc_object_t amethyst_plist = NULL;
    setuid(0);
    setgid(0);

    host_get_special_port(mach_host_self(), HOST_LOCAL_NODE, 4, &tfp0);
    if (!MACH_PORT_VALID(tfp0)) goto done;

    if ((chimera_plist = xpc_open_plist("/chimera/jailbreakd.plist")) == NULL) goto done;
    xpc_object_t chimera_env = xpc_dictionary_get_value(chimera_plist, "EnvironmentVariables");
    if (chimera_env == NULL) goto done;

    kernel_base = dict_get_u64(chimera_env, "KernelBase");
    all_proc = dict_get_u64(chimera_env, "AllProc");
    if (kernel_base == 0 || all_proc == 0) goto done;
    kernel_slide = kernel_base - 0xfffffff007004000;

    if ((amethyst_plist = xpc_open_plist("/amethyst/handoff.plist")) == NULL) goto done;
    uint64_t old_dynamic_trustcache = dict_get_u64(amethyst_plist, "dynamic_trustcache");
    uint64_t old_static_trustcache = dict_get_u64(amethyst_plist, "static_trustcache");
    uint64_t old_kernel_base = dict_get_u64(amethyst_plist, "kernel_base");
    if (old_dynamic_trustcache == 0 || old_kernel_base == 0) goto done;

    dynamic_trustcache = kernel_base + (old_dynamic_trustcache - old_kernel_base);
    if (old_static_trustcache != 0) {
        uint64_t diff = old_kernel_base - old_static_trustcache;
        static_trustcache = kernel_base - diff;
    }

    uint64_t old_pidhash_ptr = dict_get_u64(amethyst_plist, "pidhash_ptr");
    uint64_t old_pidhashtbl_ptr = dict_get_u64(amethyst_plist, "pidhashtbl_ptr");
    if (old_pidhash_ptr != 0 && old_pidhashtbl_ptr != 0) {
        pidhash_ptr = kernel_base + (old_pidhash_ptr - old_kernel_base);
        pidhashtbl_ptr = kernel_base + (old_pidhashtbl_ptr - old_kernel_base);
    }

    kern_proc_addr = find_proc_for_pid(0);
    kern_task_addr = kread64(kern_proc_addr + 0x10);
    launchd_proc_addr = find_proc_for_pid(1);
    launchd_task_addr = kread64(launchd_proc_addr + 0x10);
    self_proc_addr = find_proc_for_pid(getpid());
    self_task_addr = kread64(self_proc_addr + 0x10);

    kwrite32(self_task_addr + 0x390, kread32(self_task_addr + 0x390) | 0x400);
    kwrite64(kread64(kread64(self_proc_addr + 0xf8) + 0x78) + 0x10, 0);
    kwrite64(kread64(kread64(launchd_proc_addr + 0xf8) + 0x78) + 0x10, 0);

    uint32_t cs_flags = kread32(self_proc_addr + 0x290);
    cs_flags |= CS_INSTALLER|CS_VALID|CS_GET_TASK_ALLOW|CS_INVALID_ALLOWED|CS_PLATFORM_BINARY|CS_DEBUGGED|CS_SIGNED;
    cs_flags &= ~(CS_FORCED_LV|CS_HARD|CS_KILL|CS_RESTRICT|CS_ENFORCEMENT|CS_REQUIRE_LV);
    kwrite32(self_proc_addr + 0x290, cs_flags);

    uint32_t proc_flags = kread32(self_proc_addr + 0x13c);
    kwrite32(self_proc_addr + 0x13c, proc_flags & ~P_SUGID);

    cs_flags = kread32(launchd_proc_addr + 0x290);
    cs_flags |= CS_INSTALLER|CS_VALID|CS_GET_TASK_ALLOW|CS_INVALID_ALLOWED|CS_PLATFORM_BINARY|CS_DEBUGGED|CS_SIGNED;
    cs_flags &= ~(CS_FORCED_LV|CS_HARD|CS_KILL|CS_RESTRICT|CS_ENFORCEMENT|CS_REQUIRE_LV);
    kwrite32(launchd_proc_addr + 0x290, cs_flags);

    if (patch_dyld() != 0) goto done;
    if (access("/usr/lib/dyld_patch", F_OK) == 0) {
        swap_namecache_vnode("/usr/lib/dyld_patch", "/usr/lib/dyld");
        chmod("/usr/lib/dyld_patch", 0755);
        chown("/usr/lib/dyld_patch", 0, 0);
    }

    init_handoff_plist();
    usleep(200000);

    inject_dylib(1, "/amethyst/launchd_hook.dylib");
    usleep(2000000);
    vnode_hide_path("/chimera");

done:
    if (chimera_plist != NULL) xpc_release(chimera_plist);
    if (amethyst_plist != NULL) xpc_release(amethyst_plist);
    userspace_reboot();

    usleep(200000);
    exit(0);
    return 0;
}
