#ifndef launchd_hook_codesign_h
#define launchd_hook_codesign_h

#include "info.h"
#include "basebin_macho.h"

#define CS_VALID                                0x00000001
#define CS_ADHOC                                0x00000002
#define CS_GET_TASK_ALLOW                       0x00000004
#define CS_INSTALLER                            0x00000008
#define CS_FORCED_LV                            0x00000010
#define CS_INVALID_ALLOWED                      0x00000020
#define CS_HARD                                 0x00000100
#define CS_KILL                                 0x00000200
#define CS_CHECK_EXPIRATION                     0x00000400
#define CS_RESTRICT                             0x00000800
#define CS_ENFORCEMENT                          0x00001000
#define CS_REQUIRE_LV                           0x00002000
#define CS_ENTITLEMENTS_VALIDATED               0x00004000
#define CS_NVRAM_UNRESTRICTED                   0x00008000
#define CS_RUNTIME                              0x00010000
#define CS_EXEC_SET_HARD                        0x00100000
#define CS_EXEC_SET_KILL                        0x00200000
#define CS_EXEC_SET_ENFORCEMENT                 0x00400000
#define CS_EXEC_INHERIT_SIP                     0x00800000
#define CS_KILLED                               0x01000000
#define CS_DYLD_PLATFORM                        0x02000000
#define CS_PLATFORM_BINARY                      0x04000000
#define CS_PLATFORM_PATH                        0x08000000
#define CS_DEBUGGED                             0x10000000
#define CS_SIGNED                               0x20000000
#define CS_DEV_CODE                             0x40000000
#define CS_DATAVAULT_CONTROLLER                 0x80000000
#define CS_EXECSEG_MAIN_BINARY                  0x00000001
#define CS_EXECSEG_ALLOW_UNSIGNED               0x00000010
#define CS_EXECSEG_DEBUGGER                     0x00000020
#define CS_EXECSEG_JIT                          0x00000040
#define CS_EXECSEG_SKIP_LV                      0x00000080
#define CS_EXECSEG_CAN_LOAD_CDHASH              0x00000100
#define CS_EXECSEG_CAN_EXEC_CDHASH              0x00000200

#define CS_SIGNER_TYPE_UNKNOWN                  0x00000000
#define CSMAGIC_EMBEDDED_SIGNATURE              0xfade0cc0
#define CSSLOT_ENTITLEMENTS                     0x00000005
#define CSMAGIC_EMBEDDED_ENTITLEMENTS           0xfade7171
#define CSMAGIC_CODEDIRECTORY                   0xfade0c02
#define PAGE_SHIFT_4K                           12

typedef struct {
    uint8_t cs_type;
    size_t cs_size;
    size_t cs_digest_size;
    void *cs_init;
    void *cs_update;
    void *cs_final;
} cs_hash_t;

typedef struct __cs_blob {
    struct __cs_blob *csb_next;
    cpu_type_t csb_cpu_type;
    uint32_t csb_flags;
    off_t csb_base_offset;
    off_t csb_start_offset;
    off_t csb_end_offset;
    vm_size_t csb_mem_size;
    vm_offset_t csb_mem_offset;
    vm_address_t csb_mem_kaddr;
    uint8_t csb_cdhash[CS_CDHASH_LEN];
    cs_hash_t *csb_hashtype;
    vm_size_t csb_hash_pagesize;
    vm_size_t csb_hash_pagemask;
    vm_size_t csb_hash_pageshift;
    vm_size_t csb_hash_firstlevel_pagesize;
    CS_CodeDirectory *csb_cd;
    char *csb_teamid;
    CS_GenericBlob *csb_entitlements_blob;
    void *csb_entitlements;
    uint32_t csb_signer_type;
    uint32_t csb_reconstituted;
    union {
        struct {
            uint32_t binary;
            uint32_t path;
        } csb_platform;
        uint64_t signer_info;
    };
#ifdef __arm64e__
    uint64_t csb_pmap_cs_entry;
#endif
} cs_blob_t;

int sign_binary(char *path, uint32_t le_offset, uint32_t le_size, uint32_t slice_offset, uint32_t file_type);

#endif /* launchd_hook_codesign_h */


