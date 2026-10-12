#ifndef basebins_common_h
#define basebins_common_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <time.h>
#include <spawn.h>
#include <pthread.h>
#include <libgen.h>
#include <limits.h>
#include <_ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/dirent.h>
#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <xpc/xpc.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <sys/utsname.h>
#include <sys/attr.h>
#include <sys/fsgetpath.h>
#include <pwd.h>
#include <CommonCrypto/CommonDigest.h>
#include <pthread.h>

typedef struct __attribute__((packed)) {
    uint32_t mach_init_symbol;
    uint32_t cc_sha1_symbol;
    uint32_t cc_sha256_symbol;
    uint32_t cc_sha384_symbol;
    uint32_t ccdigest_init_symbol;
    uint32_t ccdigest_update_symbol;
    uint32_t cc_clear_symbol;
    uint32_t getenv_symbol;
    uint32_t strstr_symbol;
    uint32_t dyld_start_symbol;
    uint32_t mach_task_self_symbol;
    uint32_t errno_symbol;
    uint32_t sb_ext_offset;
    uint32_t page_size;
    uint32_t arm64e_host;
    uint8_t ios_version[4];
    uint64_t ns_init_platform_symbol;
    uint64_t use_dyld3_symbol;
    char dyld_insert_str[32];
    char base_hook_str[32];
    char sb_policy_str[32];
} dyld_patch_handoff_t;

typedef struct {
    dyld_patch_handoff_t *dyld_handoff;
    uint32_t ios_version[3];
    uint64_t dsc_slide;
    uint32_t page_size;
    char *sb_ext_list;
    char *exec_path;
    char *proc_name;
    bool is_xpcproxy;
    bool arm64e_host;
    bool stock_libswift;
    bool dyld3_mode;
    bool init_done;
} basebin_info_t;

typedef struct {
    uint32_t version;
    uint32_t infoArrayCount;
    const struct dyld_image_info *infoArray;
    dyld_image_notifier notification;
    bool processDetachedFromSharedRegion;
    bool libSystemInitialized;
    const struct mach_header *dyldImageLoadAddress;
    void *jitInfo;
    const char *dyldVersion;
    const char *errorMessage;
    uintptr_t terminationFlags;
    void *coreSymbolicationShmPage;
    uintptr_t systemOrderFlag;
    uintptr_t uuidArrayCount;
    const struct dyld_uuid_info *uuidArray;
    struct dyld_all_image_infos *dyldAllImageInfosAddress;
    uintptr_t initialImageCount;
    uintptr_t errorKind;
    const char *errorClientOfDylibPath;
    const char *errorTargetDylibPath;
    const char *errorSymbol;
    uintptr_t sharedCacheSlide;
    uint8_t sharedCacheUUID[16];
    uintptr_t sharedCacheBaseAddress;
    uint64_t infoArrayChangeTimestamp;
    const char *dyldPath;
    mach_port_t notifyPorts[DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT];
    uintptr_t reserved[11-(DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT/2)];
    uint64_t sharedCacheFSID;
    uint64_t sharedCacheFSObjID;
    uintptr_t compact_dyld_image_info_addr;
    size_t compact_dyld_image_info_size;
    uint32_t platform;
    uint32_t aotInfoCount;
    const struct dyld_aot_image_info* aotInfoArray;
    uint64_t aotInfoArrayChangeTimestamp;
    uintptr_t aotSharedCacheBaseAddress;
    uint8_t aotSharedCacheUUID[16];
} dyld_all_image_infos_t;

struct _os_alloc_once_s {
    long once;
    void *ptr;
};

struct xpc_global_data {
    uint64_t a;
    uint64_t xpc_flags;
    mach_port_t task_bootstrap_port;
#ifndef __LP64__
    uint32_t padding;
#endif
    xpc_object_t xpc_bootstrap_pipe;
};

typedef void *xpc_pipe_t;
typedef struct section_64 section_t;
typedef struct segment_command_64 segment_command_t;
typedef struct mach_header_64 mach_header_t;
typedef struct nlist_64 nlist_t;

extern int posix_spawnattr_getprocesstype_np(posix_spawnattr_t *, int *);
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_name(int pid, void *buffer, uint32_t buffersize);
extern int csops(pid_t pid, unsigned int  ops, void *useraddr, size_t usersize);
extern int ptrace(int request, pid_t pid, uint64_t addr, int data);
extern char ***_NSGetArgv(void);
extern char ***_NSGetEnviron(void);
extern char **_NSGetProgname(void);
mach_header_t *_NSGetMachExecuteHeader(void);
extern char **environ;

extern struct _os_alloc_once_s _os_alloc_once_table[];
extern int xpc_pipe_routine(xpc_object_t pipe, xpc_object_t message, xpc_object_t *reply);
extern int xpc_pipe_receive(mach_port_t port, xpc_object_t *message);
extern void* _os_alloc_once(struct _os_alloc_once_s *slot, size_t sz, os_function_t init);
extern xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, uint32_t flags);
extern void xpc_dictionary_get_audit_token(xpc_object_t xdict, audit_token_t *token);
extern int xpc_pipe_routine_reply(xpc_object_t reply);
extern xpc_object_t xpc_create_from_plist(void *data, size_t size);
extern int xpc_pipe_routine_with_flags(xpc_object_t xpc_pipe, xpc_object_t inDict, XPC_GIVES_REFERENCE xpc_object_t *reply, uint32_t flags);
extern char *xpc_strerror(int err);
extern mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t xdict, const char *key);
extern void xpc_dictionary_set_mach_send(xpc_object_t xdict, const char *key, mach_port_t p);

extern int sandbox_check(pid_t pid, const char *operation, uint32_t type, ...);
extern int64_t sandbox_extension_consume(const char *extension_token);
extern int sandbox_check_by_audit_token(audit_token_t at, const char *op, int filter, ...);

extern basebin_info_t basebin_info;

int basebin_init(char **apple);

#endif /* basebins_common_h */
