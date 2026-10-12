#ifndef dyld_patch_h
#define dyld_patch_h

#include "common.h"

#define CS_CDHASH_LEN                           0x00000014
#define CS_HASHTYPE_SHA1                        0x00000001
#define CS_HASHTYPE_SHA256                      0x00000002
#define CS_HASHTYPE_SHA256_TRUNCATED            0x00000003
#define CS_HASHTYPE_SHA384                      0x00000004

#define CSSLOT_CODEDIRECTORY                    0x00000000
#define CSSLOT_INFOSLOT                         0x00000001
#define CSSLOT_REQUIREMENTS                     0x00000002
#define CSSLOT_RESOURCEDIR                      0x00000003
#define CSSLOT_APPLICATION                      0x00000004
#define CSSLOT_ENTITLEMENTS                     0x00000005
#define CSSLOT_DER_ENTITLEMENTS                 0x00000007
#define CSSLOT_LAUNCH_CONSTRAINT_SELF           0x00000008
#define CSSLOT_LAUNCH_CONSTRAINT_PARENT         0x00000009
#define CSSLOT_LAUNCH_CONSTRAINT_RESPONSIBLE    0x0000000a
#define CSSLOT_LIBRARY_CONSTRAINT               0x0000000b
#define CSSLOT_SIGNATURESLOT                    0x00010000
#define CSSLOT_IDENTIFICATIONSLOT               0x00010001
#define CSSLOT_TICKETSLOT                       0x00010002
#define CSSLOT_ALTERNATE_CODEDIRECTORIES        0x00001000
#define CSSLOT_ALTERNATE_CODEDIRECTORY_MAX      0x00000005
#define CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT    0x00001005

#define CS_SIGNER_TYPE_UNKNOWN                  0x00000000
#define CSMAGIC_EMBEDDED_SIGNATURE              0xfade0cc0
#define CSSLOT_ENTITLEMENTS                     0x00000005
#define CSMAGIC_EMBEDDED_ENTITLEMENTS           0xfade7171
#define CSMAGIC_CODEDIRECTORY                   0xfade0c02

#define MACHO_VALID(m) (m==MH_MAGIC_64||m==MH_CIGAM_64||m==MH_MAGIC||m==MH_CIGAM||m==FAT_MAGIC_64||m==FAT_CIGAM_64||m==FAT_MAGIC||m==FAT_CIGAM)
#define MACHO_REQUIRES_SWAP(m) (m==MH_CIGAM_64||m==MH_CIGAM||m==FAT_CIGAM_64||m==FAT_CIGAM)
#define MACHO_FAT(m) (m==FAT_MAGIC_64||m==FAT_CIGAM_64||m==FAT_MAGIC||m==FAT_CIGAM)
#define MACHO_32BIT(m) (m==MH_MAGIC||m==MH_CIGAM)
#define DYLIB_LOADCMD(c) (c==LC_LOAD_DYLIB||c==LC_LOAD_WEAK_DYLIB||c==LC_REEXPORT_DYLIB||c==LC_LOAD_UPWARD_DYLIB)

#ifndef CPU_SUBTYPE_MASK
#define CPU_SUBTYPE_MASK 0xff000000
#endif

#ifndef CPU_SUBTYPE_LIB64
#define CPU_SUBTYPE_LIB64 0x80000000
#endif

#ifndef CPU_SUBTYPE_PTRAUTH_ABI
#define CPU_SUBTYPE_PTRAUTH_ABI 0x80000000
#endif

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t length;
    uint32_t version;
    uint32_t flags;
    uint32_t hashOffset;
    uint32_t identOffset;
    uint32_t nSpecialSlots;
    uint32_t nCodeSlots;
    uint32_t codeLimit;
    uint8_t hashSize;
    uint8_t hashType;
    uint8_t platform;
    uint8_t pageSize;
    uint32_t spare2;
    char end_earliest[0];
    uint32_t scatterOffset;
    char end_withScatter[0];
    uint32_t teamOffset;
    char end_withTeam[0];
    uint32_t spare3;
    uint64_t codeLimit64;
    char end_withCodeLimit64[0];
    uint64_t execSegBase;
    uint64_t execSegLimit;
    uint64_t execSegFlags;
    char end_withExecSeg[0];
} CS_CodeDirectory;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t length;
    uint32_t version;
    uint32_t flags;
    uint32_t hashOffset;
    uint32_t identOffset;
    uint32_t nSpecialSlots;
    uint32_t nCodeSlots;
    uint32_t codeLimit;
    uint8_t hashSize;
    uint8_t hashType;
    uint8_t platform;
    uint8_t pageSize;
    uint32_t spare2;
    char end_earliest[0];
} CS_CodeDirectory_legacy;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t offset;
} CS_BlobIndex;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t length;
    uint32_t count;
    CS_BlobIndex index[];
} CS_SuperBlob;

typedef struct {
    uint32_t magic;
    uint32_t length;
    char data[];
} CS_GenericBlob;

typedef struct {
    uint32_t count;
    uint32_t base;
    uint64_t targetOffset;
    uint64_t spare;
} SC_Scatter;

typedef struct {
    char magic[16];
    uint32_t mappingOffset;
    uint32_t mappingCount;
    uint32_t imagesOffsetLegacy;
    uint32_t imagesCountLegacy;
    uint64_t dyldBaseAddress;
    uint64_t codeSignatureOffset;
    uint64_t codeSignatureSize;
    uint64_t slideInfoOffsetUnused;
    uint64_t slideInfoSizeUnused;
    uint64_t localSymbolsOffset;
    uint64_t localSymbolsSize;
    uint8_t uuid[16];
    uint64_t cacheType;
    uint32_t branchPoolsOffset;
    uint32_t branchPoolsCount;
    uint64_t accelerateInfoAddr;
    uint64_t accelerateInfoSize;
    uint64_t imagesTextOffset;
    uint64_t imagesTextCount;
    uint64_t patchInfoAddr;
    uint64_t patchInfoSize;
    uint64_t otherImageGroupAddrUnused;
    uint64_t otherImageGroupSizeUnused;
    uint64_t progClosuresAddr;
    uint64_t progClosuresSize;
    uint64_t progClosuresTrieAddr;
    uint64_t progClosuresTrieSize;
    uint32_t platform;
    uint32_t formatVersion:8,
        dylibsExpectedOnDisk:1,
        simulator:1,
        locallyBuiltCache:1,
        builtFromChainedFixups:1,
        padding: 20;
    uint64_t sharedRegionStart;
    uint64_t sharedRegionSize;
    uint64_t maxSlide;
    uint64_t dylibsImageArrayAddr;
    uint64_t dylibsImageArraySize;
    uint64_t dylibsTrieAddr;
    uint64_t dylibsTrieSize;
    uint64_t otherImageArrayAddr;
    uint64_t otherImageArraySize;
    uint64_t otherTrieAddr;
    uint64_t otherTrieSize;
    uint32_t mappingWithSlideOffset;
    uint32_t mappingWithSlideCount;
    uint64_t dylibsPBLStateArrayAddrUnused;
    uint64_t dylibsPBLSetAddr;
    uint64_t programsPBLSetPoolAddr;
    uint64_t programsPBLSetPoolSize;
    uint64_t programTrieAddr;
    uint32_t programTrieSize;
    uint32_t osVersion;
    uint32_t altPlatform;
    uint32_t altOsVersion;
    uint64_t swiftOptsOffset;
    uint64_t swiftOptsSize;
    uint32_t subCacheArrayOffset;
    uint32_t subCacheArrayCount;
    uint8_t symbolFileUUID[16];
    uint64_t rosettaReadOnlyAddr;
    uint64_t rosettaReadOnlySize;
    uint64_t rosettaReadWriteAddr;
    uint64_t rosettaReadWriteSize;
    uint32_t imagesOffset;
    uint32_t imagesCount;
    uint32_t cacheSubType;
    uint64_t objcOptsOffset;
    uint64_t objcOptsSize;
    uint64_t cacheAtlasOffset;
    uint64_t cacheAtlasSize;
    uint64_t dynamicDataOffset;
    uint64_t dynamicDataMaxSize;
} dyld_cache_header_t;

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

typedef struct  {
    uint32_t nlistOffset;
    uint32_t nlistCount;
    uint32_t stringsOffset;
    uint32_t stringsSize;
    uint32_t entriesOffset;
    uint32_t entriesCount;
} dyld_cache_local_symbols_info_t;

typedef struct {
    uint64_t address;
    uint64_t modTime;
    uint64_t inode;
    uint32_t pathFileOffset;
    uint32_t pad;
} dyld_cache_image_info_t;

typedef struct  {
    uint32_t dylibOffset;
    uint32_t nlistStartIndex;
    uint32_t nlistCount;
} dyld_cache_local_symbols_entry_t;

typedef struct  {
    uint64_t dylibOffset;
    uint32_t nlistStartIndex;
    uint32_t nlistCount;
} dyld_cache_local_symbols_entry_64_t;

typedef struct {
    void *aligned_data;
    uint32_t aligned_size;
    void *data;
    uint32_t size;
} dyld_cache_ranged_mapping_t;

typedef struct {
    dyld_cache_header_t *cache_hdr;
    uintptr_t cache_slide;
    uintptr_t dyld_base;
    dyld_cache_ranged_mapping_t *entry_mapping;
    dyld_cache_ranged_mapping_t *symbol_mapping;
    dyld_cache_ranged_mapping_t *string_mapping;
    dyld_cache_local_symbols_info_t symbol_info;
    bool legacy_entries;
} dyld_info_t;

typedef struct {
    union {
        struct mach_header_64 *hdr64;
        uint8_t *hdr;
    };
    struct load_command *load_cmd;
    uint32_t cmd_count;
    uint32_t offset;
    uint32_t size;
    uint8_t *text_data;
    uint32_t text_size;
    bool has_pac;
} dyld_slice_t;

typedef struct {
    uint8_t *file_data;
    uint32_t file_size;
    dyld_slice_t arm64;
    dyld_slice_t arm64e;
} dyld_patch_ctx_t;

typedef struct __attribute__((packed)) {
    char sb_ext1_str[224];
    char sb_ext2_str[224];
    char sb_ext3_str[224];
    char sb_ext4_str[224];
    char sb_ext5_str[224];
    char sb_ext6_str[224];
    uint64_t ___padding;
} dyld_patch_sb_ext_t;

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
} dyld_patch_info_t;

extern const char *dyld_shared_cache_file_path(void);
extern int64_t sandbox_extension_consume(const char *extension_token);
extern char *sandbox_extension_issue_file(const char *extension_class, const char *path, uint32_t flags);

#define SANDBOX_READ "com.apple.app-sandbox.read"
#define SANDBOX_READ_WRITE "com.apple.app-sandbox.read-write"
#define SANDBOX_EXECUTABLE "com.apple.sandbox.executable"

int patch_dyld(void);

#endif /* dyld_h */
