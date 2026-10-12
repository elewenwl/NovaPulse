#ifndef launchd_hook_trustcache_h
#define launchd_hook_trustcache_h

#include "info.h"

#define TRUSTCACHE_MAGIC    0x414d455448595354

typedef struct __attribute__((packed)) {
    uint64_t next;
    uuid_t uuid;
    uint32_t count;
} dynamic_trustcache_hdr_v0_t;

typedef struct __attribute__((packed)) {
    uint32_t version;
    uuid_t uuid;
    uint32_t count;
} dynamic_trustcache_hdr_v1_t;

typedef struct __attribute__((packed)) {
    uint64_t next;
    uint64_t list;
} dynamic_trustcache_list_t;

typedef struct __attribute__((packed)) {
    uint8_t cd_hash[20];
} dynamic_trustcache_entry_v0_t;

typedef struct __attribute__((packed)) {
    uint8_t cd_hash[20];
    uint8_t hash_type;
    uint8_t flags;
} dynamic_trustcache_entry_v1_t;

typedef struct __attribute__((packed)) {
    dynamic_trustcache_list_t list;
    dynamic_trustcache_hdr_v1_t hdr;
    dynamic_trustcache_entry_v1_t entry;
    uint8_t __padding[2];
} dynamic_trustcache_full_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t version;
    uuid_t uuid;
    uint32_t count;
} static_trustcache_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t cd_hash[20];
    uint8_t hash_type;
    uint8_t flags;
} static_trustcache_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t cd_hash[20];
} generic_trustcache_entry_t;

typedef enum {
    STATIC_TC_TYPE_UNKNOWN = -1,
    STATIC_TC_TYPE_SEGEXTRADATA,
    STATIC_TC_TYPE_AMFI_KEXT,
    STATIC_TC_TYPE_IMG4_FILE
} static_trustcache_type_t;

typedef enum {
    DYNAMIC_TC_TYPE_UNKNOWN = -1,
    DYNAMIC_TC_TYPE_V0,
    DYNAMIC_TC_TYPE_V1
} dynamic_trustcache_type_t;

typedef struct __attribute__((packed)) {
    uint8_t cd_hash[20];
    uint8_t hash_type;
} recent_trustcache_entry_t;

typedef struct {
    generic_trustcache_entry_t *static_entries;
    uint32_t static_count;
    dynamic_trustcache_type_t dynamic_type;
    uint64_t dynamic_hdr;
    uint64_t dynamic_entries;
    uint32_t dynamic_count;
    uint32_t dynamic_size;
    union {
        void *data;
        dynamic_trustcache_entry_v0_t *v0;
        dynamic_trustcache_full_entry_t *v1;
    } local_entries;
    bool entries_stale;
} trustcache_info_t;

int trustcache_init(void);
bool trustcache_static_check(uint8_t *cd_hash);
bool trustcache_dynamic_check(uint8_t *cd_hash);
bool trustcache_check(uint8_t *cd_hash);
uint64_t trustcache_find_slot(void);
int trustcache_add_hash(uint8_t *cd_hash, uint8_t hash_type);
int trustcache_add_binary(const char *path);
bool trustcache_lock_check(uint8_t *cd_hash);
int trustcache_lock_add_hash(uint8_t *cd_hash, uint8_t hash_type);
int trustcache_lock_add_binary(const char *path);

#endif /* launchd_hook_trustcache_h */
