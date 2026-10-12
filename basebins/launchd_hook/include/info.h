#ifndef launchd_hook_info_h
#define launchd_hook_info_h

#include "basebin_common.h"
#include <CoreFoundation/CoreFoundation.h>
#include <stdatomic.h>

#define TF_PLATFORM                             0x00000400
#define CS_OPS_STATUS                           0x00000000
#define PROC_PIDPATHINFO                        11
#define PROC_PIDPATHINFO_SIZE                   1024
#define PROC_PIDPATHINFO_MAXSIZE                (4*1024)
#define PROC_ALL_PIDS                           1
#define RB2_USERREBOOT                          0x2000000000000000LLU
#define PID_MAX                                 99999
#define PID_MIN                                 0

#define koffsetof(struct, entry) kinfo->offsets.struct.entry

typedef struct {
    uint64_t self_task_addr;
    uint64_t self_proc_addr;
    uint64_t self_port_addr;
    uint64_t kern_task_addr;
    uint64_t kern_proc_addr;
    uint64_t kernel_base;
    uint64_t kernel_slide;
    uint32_t pmap_page_size;
    uint32_t vm_page_size;
    mach_port_t tfp0;
    uint64_t self_ttep;
    uint64_t cpu_ttep;
    uint32_t version[3];
    volatile uint32_t initialized;
    volatile uint32_t userspace_rebooting;
    volatile uint32_t shutting_down;
    volatile uint32_t first_run;
    volatile uint32_t using_tnsv2;
    
    struct {
        bool kpp;
        bool ktrr;
        bool pan;
        bool pac;
        bool ppl;
    } protections;
    
    struct {
        struct {
            uint32_t bsd_info;
            uint32_t vm_map;
            uint32_t next;
            uint32_t itk_space;
            uint32_t t_flags;
            uint32_t transfer_memory_ownership;
        } task;

        struct {
            uint32_t task;
            uint32_t pid;
            uint32_t p_fd;
            uint32_t p_flag;
            uint32_t p_svuid;
            uint32_t p_svgid;
            uint32_t ucred;
            uint32_t csflags;
            uint32_t p_textvp;
            uint32_t p_hash;
        } proc;
        
        struct {
            uint32_t cr_uid;
            uint32_t cr_ruid;
            uint32_t cr_svuid;
            uint32_t cr_groups;
            uint32_t cr_rgid;
            uint32_t cr_svgid;
            uint32_t cr_label;
        } ucred;
        
        struct {
            uint32_t slots;
        } label;
        
        struct {
            uint32_t fd_ofiles;
        } filedesc;
        
        struct {
            uint32_t f_fglob;
        } fileproc;
        
        struct {
            uint32_t fg_data;
        } fileglob;
        
        struct {
            uint32_t ip_bits;
            uint32_t ikmq_base;
            uint32_t ip_receiver;
            uint32_t ip_kobject;
        } ipc_port;
        
        struct {
            uint32_t is_table_size;
            uint32_t is_table;
        } ipc_space;
        
        struct {
            uint32_t namecache;
            uint32_t ubcinfo;
        } vnode;

        struct {
            uint32_t flags;
        } specinfo;

        struct {
            uint32_t cs_blob;
        } ubc_info;
        
        struct {
            uint32_t vnode;
        } namecache;
        
        struct {
            uint32_t pmap;
            uint32_t flags;
        } vm_map;
        
        struct {
            uint32_t ttep;
            uint32_t cs_enforced;
        } pmap;

        struct {
            uint32_t trust;
        } pmap_cs_code_directory;
    } offsets;
    
    struct {
        uint64_t dynamic_trustcache;
        uint64_t static_trustcache;
        uint64_t ptov_table;
        uint64_t pidhash_ptr;
        uint64_t pidhashtbl_ptr;
        uint64_t gPhysBase;
        uint64_t gVirtBase;
        uint64_t pplrw_entry;
        uint64_t pplrw_mapping_va;
        uint64_t pplrw_mapping_pa;
    } patches;
} kinfo_t;

extern CFDictionaryRef _CFCopySystemVersionDictionary(void);
extern kinfo_t *kinfo;

#endif /* launchd_hook_info_h */