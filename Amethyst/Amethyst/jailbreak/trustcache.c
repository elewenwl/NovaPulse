#include "jailbreak.h"
#include "macho.h"
#include "trustcache.h"
#include "dma.h"
#include "ppl.h"

static trustcache_info_t *tc_info = NULL;

int trustcache_init(void) {
    if ((tc_info = calloc(1, sizeof(trustcache_info_t))) == NULL) goto err;
    static_trustcache_type_t static_type = STATIC_TC_TYPE_UNKNOWN;
    dynamic_trustcache_type_t dynamic_type = DYNAMIC_TC_TYPE_UNKNOWN;

    if (kinfo->patches.static_trustcache == 0) {
        if (access("/usr/standalone/firmware/FUD/StaticTrustCache.img4", F_OK) == 0) {
            static_type = STATIC_TC_TYPE_IMG4_FILE;
        }
    } else {
        if (kinfo->version[0] >= 12) {
            static_type = STATIC_TC_TYPE_SEGEXTRADATA;
        } else {
            static_type = STATIC_TC_TYPE_AMFI_KEXT;
        }
    }
    
    if (kinfo->patches.dynamic_trustcache != 0) {
        if (kinfo->version[0] >= 13) {
            dynamic_type = DYNAMIC_TC_TYPE_V1;
        } else {
            dynamic_type = DYNAMIC_TC_TYPE_V0;
        }
    }
    
    if (static_type == STATIC_TC_TYPE_UNKNOWN || dynamic_type == DYNAMIC_TC_TYPE_UNKNOWN) goto err;
    switch (static_type) {
        case STATIC_TC_TYPE_SEGEXTRADATA: {
            static_trustcache_hdr_t hdr = {0};
            kread_buf(kinfo->patches.static_trustcache, &hdr, sizeof(static_trustcache_hdr_t));
            if (hdr.version != 1 || hdr.count <= 32 || hdr.count >= UINT16_MAX) goto err;
            
            tc_info->static_count = hdr.count;
            tc_info->static_entries = calloc(1, sizeof(generic_trustcache_entry_t) * (tc_info->static_count + 1));
            if (tc_info->static_entries == NULL) goto err;
            
            uint64_t entry_start = kinfo->patches.static_trustcache + sizeof(static_trustcache_hdr_t);
            for (uint32_t i = 0; i < tc_info->static_count; i++) {
                static_trustcache_entry_t entry = {0};
                uint64_t offset = sizeof(static_trustcache_entry_t) * i;
                kread_buf(entry_start + offset, &entry, sizeof(static_trustcache_entry_t));
                memcpy(&tc_info->static_entries[i].cd_hash[0], &entry.cd_hash[0], 20);
            }
        } break;
            
        case STATIC_TC_TYPE_AMFI_KEXT: {
            uint64_t table_start = kinfo->patches.static_trustcache;
            uint64_t entry_start = table_start + 0x400;
            uint32_t entry_idx = 0;
            tc_info->static_count = 0;
            
            for (uint32_t i = 0; i < 0xff; i++) {
                uint16_t group_count = kread16(kinfo->patches.static_trustcache + (i * 0x4));
                if (group_count <= 1024) tc_info->static_count += group_count;
            }
            
            if (tc_info->static_count <= 32 || tc_info->static_count >= UINT16_MAX) goto err;
            tc_info->static_entries = calloc(1, sizeof(generic_trustcache_entry_t) * (tc_info->static_count + 1));
            if (tc_info->static_entries == NULL) goto err;
            
            for (uint32_t i = 0; i < 0xff; i++) {
                uint16_t group_count = kread16(table_start + (i * 0x4));
                if (group_count == 0) continue;
                
                uint16_t group_idx = kread16(table_start + (i * 0x4) + 0x2);
                for (uint32_t j = 0; j < group_count; j++) {
                    tc_info->static_entries[entry_idx].cd_hash[0] = i;
                    uint64_t hash_addr = entry_start + (group_idx * 19) + (j * 19);
                    kread_buf(hash_addr, &tc_info->static_entries[entry_idx].cd_hash[1], 19);
                    entry_idx++;
                }
            }
            
            tc_info->static_count = entry_idx;
            if (tc_info->static_count == 0) goto err;
        } break;
            
        case STATIC_TC_TYPE_IMG4_FILE: {
            int fd = open("/usr/standalone/firmware/FUD/StaticTrustCache.img4", O_RDONLY);
            if (fd < 0) goto err;
            
            struct stat st = {0};
            fstat(fd, &st);
            
            uint8_t *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            close(fd);
            if (data == MAP_FAILED) goto err;
            
            uint8_t *trst = (uint8_t *)strnstr((char *)data, "trst", st.st_size);
            if (trst == NULL) {
                munmap(data, st.st_size);
                goto err;
            }
            
            uint16_t len = *(uint16_t *)(trst + 0x8);
            uint32_t ver = 0x00000001;
            uint8_t *hdr_data = memmem((char *)trst + 0x8, 0x20, &ver, 0x4);
            if (hdr_data == NULL || len == 0) {
                munmap(data, st.st_size);
                goto err;
            }
            
            static_trustcache_hdr_t *hdr = (static_trustcache_hdr_t *)hdr_data;
            if (hdr->version != 1 || hdr->count <= 32 || hdr->count >= UINT16_MAX) {
                munmap(data, st.st_size);
                goto err;
            }
            
            tc_info->static_count = hdr->count;
            tc_info->static_entries = calloc(1, sizeof(generic_trustcache_entry_t) * (tc_info->static_count + 1));
            if (tc_info->static_entries == NULL) goto err;
            
            uint8_t *entry_start = hdr_data + sizeof(static_trustcache_hdr_t);
            for (uint32_t i = 0; i < tc_info->static_count; i++) {
                uint64_t offset = sizeof(static_trustcache_entry_t) * i;
                static_trustcache_entry_t *entry = (static_trustcache_entry_t *)(entry_start + offset);
                memcpy(&tc_info->static_entries[i].cd_hash[0], &entry->cd_hash[0], 20);
            }
            munmap(data, st.st_size);
        } break;
        default: goto err;
    }
    
    switch (dynamic_type) {
        case DYNAMIC_TC_TYPE_V0: {
            tc_info->dynamic_count = 4000;
            tc_info->dynamic_size = sizeof(dynamic_trustcache_hdr_v0_t) + (sizeof(dynamic_trustcache_entry_v0_t) * tc_info->dynamic_count);
            uint64_t next = kread64(kinfo->patches.dynamic_trustcache);
            
            if (next == 0 || kread64(next + sizeof(dynamic_trustcache_hdr_v0_t)) != TRUSTCACHE_MAGIC) {
                dynamic_trustcache_hdr_v0_t *dynamic_trustcache = calloc(1, tc_info->dynamic_size);
                dynamic_trustcache->next = next;
                dynamic_trustcache->count = tc_info->dynamic_count;
                arc4random_buf(dynamic_trustcache->uuid, 16);
                
                *(uint64_t *)((void *)dynamic_trustcache + sizeof(dynamic_trustcache_hdr_v0_t)) = TRUSTCACHE_MAGIC;
                memset((void *)dynamic_trustcache + sizeof(dynamic_trustcache_hdr_v0_t) + 20, 0x41, tc_info->dynamic_count * 20);
                
                tc_info->dynamic_hdr = kalloc_wired(tc_info->dynamic_size);
                if (tc_info->dynamic_hdr == 0) goto err;
                kwrite_buf(tc_info->dynamic_hdr, (void *)dynamic_trustcache, tc_info->dynamic_size);
                free(dynamic_trustcache);

                if (kinfo->protections.ppl) {
                    uint64_t pa = kvtophys(kinfo->patches.dynamic_trustcache);
                    if (pa == 0) goto err;
                    ppl_write64(pa, tc_info->dynamic_hdr);
                } else {
                    kwrite64(kinfo->patches.dynamic_trustcache, tc_info->dynamic_hdr);
                }
            } else {
                tc_info->dynamic_hdr = next;
            }
            
            tc_info->dynamic_entries = tc_info->dynamic_hdr + sizeof(dynamic_trustcache_hdr_v0_t) + 20;
            tc_info->dynamic_type = DYNAMIC_TC_TYPE_V0;
        } break;
            
        case DYNAMIC_TC_TYPE_V1: {
            uint32_t entry_size = (uint32_t)sizeof(dynamic_trustcache_full_entry_t);
            tc_info->dynamic_count = 4000;
            tc_info->dynamic_size = (entry_size * tc_info->dynamic_count);
            
            dynamic_trustcache_list_t current_list = {0};
            uint64_t next = kread64(kinfo->patches.dynamic_trustcache);
            
            if (next != 0) {
                kread_buf(next, &current_list, sizeof(dynamic_trustcache_list_t));
                if (current_list.list != 0) {
                    dynamic_trustcache_hdr_v1_t current_hdr = {0};
                    kread_buf(current_list.list, &current_hdr, sizeof(dynamic_trustcache_hdr_v1_t));

                    if (*(uint64_t *)current_hdr.uuid == TRUSTCACHE_MAGIC) {
                        tc_info->dynamic_entries = next;
                    }
                }
            }
            
            if (tc_info->dynamic_entries == 0) {
                dynamic_trustcache_full_entry_t *user_entries = calloc(1, tc_info->dynamic_size);
                if (user_entries == NULL) goto err;
                
                uint64_t kern_entries = kalloc_wired(tc_info->dynamic_size);
                if (kern_entries == 0) {
                    free(user_entries);
                    goto err;
                }
                
                kzero(kern_entries, tc_info->dynamic_size);
                for (uint32_t i = 0; i < tc_info->dynamic_count; i++) {
                    uint64_t current_entry_addr = kern_entries + (sizeof(dynamic_trustcache_full_entry_t) * i);
                    uint64_t next_entry_addr = current_entry_addr + sizeof(dynamic_trustcache_full_entry_t);
                    if (i == (tc_info->dynamic_count-1)) {
                        next_entry_addr = next;
                    }
                    
                    user_entries[i].list.next = next_entry_addr;
                    user_entries[i].list.list = current_entry_addr + sizeof(dynamic_trustcache_list_t);

                    *(uint64_t *)user_entries[i].hdr.uuid = TRUSTCACHE_MAGIC;
                    user_entries[i].hdr.version = 1;
                    user_entries[i].hdr.count = 1;
                    memset(user_entries[i].entry.cd_hash, 0x41, 20);
                }
                
                tc_info->dynamic_entries = kern_entries;
                kwrite_buf(kern_entries, user_entries, tc_info->dynamic_size);
                free(user_entries);

                if (kinfo->protections.ppl) {
                    uint64_t pa = kvtophys(kinfo->patches.dynamic_trustcache);
                    if (pa == 0) goto err;
                    ppl_write64(pa, kern_entries);
                } else {
                    kwrite64(kinfo->patches.dynamic_trustcache, kern_entries);
                }
            }
            tc_info->dynamic_type = DYNAMIC_TC_TYPE_V1;
        } break;
        default: goto err;
    }
    return 0;
    
err:
    if (tc_info != NULL) {
        if (tc_info->static_entries != NULL) free(tc_info->static_entries);
        free(tc_info);
        tc_info = NULL;
    }
    return -1;
}

bool trustcache_static_check(uint8_t *cd_hash) {
    if (tc_info == NULL || tc_info->static_entries == NULL || cd_hash == NULL) return false;
    for (uint32_t i = 0; i < tc_info->static_count; i++) {
        if (memcmp(tc_info->static_entries[i].cd_hash, cd_hash, 20) == 0) return true;
    }
    return false;
}

bool trustcache_dynamic_check(uint8_t *cd_hash) {
    if (tc_info == NULL || tc_info->dynamic_entries == 0 || cd_hash == NULL) return false;
    for (int i = 0; i < tc_info->dynamic_count; i++) {
        if (tc_info->dynamic_type == DYNAMIC_TC_TYPE_V1) {
            dynamic_trustcache_full_entry_t entry = {0};
            kread_buf(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_full_entry_t) * i), &entry, sizeof(dynamic_trustcache_full_entry_t));
            if (memcmp(&entry.entry.cd_hash[0], cd_hash, 20) == 0) return true;
        } else {
            dynamic_trustcache_entry_v0_t entry = {0};
            kread_buf(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_entry_v0_t) * i), &entry, sizeof(dynamic_trustcache_entry_v0_t));
            if (memcmp(&entry.cd_hash[0], cd_hash, 20) == 0) return true;
        }
    }
    return false;
}

bool trustcache_check(uint8_t *cd_hash) {
  //  if (trustcache_static_check(cd_hash)) return true;
    return trustcache_dynamic_check(cd_hash);
}

uint64_t trustcache_find_slot(void) {
    if (tc_info == NULL || tc_info->dynamic_entries == 0) return 0;
    for (int i = 0; i < tc_info->dynamic_count; i++) {
        if (tc_info->dynamic_type == DYNAMIC_TC_TYPE_V1) {
            dynamic_trustcache_full_entry_t entry = {0};
            kread_buf(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_full_entry_t) * i), &entry, sizeof(dynamic_trustcache_full_entry_t));
            
            if (*(uint64_t *)entry.entry.cd_hash == 0x4141414141414141) {
                return tc_info->dynamic_entries + (sizeof(dynamic_trustcache_full_entry_t) * i) + offsetof(dynamic_trustcache_full_entry_t, entry.cd_hash);
            }
        } else {
            if (kread32(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_entry_v0_t) * i)) == 0x41414141) {
                return tc_info->dynamic_entries + (sizeof(dynamic_trustcache_entry_v0_t) * i);
            }
        }
    }
    return 0;
}

int trustcache_add_hash(uint8_t *cd_hash, uint8_t hash_type) {
    if (tc_info == NULL || tc_info->dynamic_entries == 0 || cd_hash == NULL) return -1;
    uint64_t slot = trustcache_find_slot();
    if (slot == 0) return -1;
    
    kwrite_buf(slot, cd_hash, 20);
    if (tc_info->dynamic_type == DYNAMIC_TC_TYPE_V1) {
        if (hash_type <= 0) hash_type = CS_HASHTYPE_SHA256;
        kwrite8(slot + 20, hash_type);
    }
    return 0;
}

void trustcache_remove_hash(uint8_t *cd_hash) {
    if (tc_info == NULL || tc_info->dynamic_entries == 0 || cd_hash == NULL) return;
    for (int i = 0; i < tc_info->dynamic_count; i++) {
        if (tc_info->dynamic_type == DYNAMIC_TC_TYPE_V1) {
            dynamic_trustcache_full_entry_t entry = {0};
            kread_buf(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_full_entry_t) * i), &entry, sizeof(dynamic_trustcache_full_entry_t));
            
            if (memcmp(&entry.entry.cd_hash[0], cd_hash, 20) == 0) {
                memset(&entry.entry.cd_hash[0], 0x41, 20);
                entry.entry.flags = 0;
                entry.entry.hash_type = 0;
            }
        } else {
            dynamic_trustcache_entry_v0_t entry = {0};
            kread_buf(tc_info->dynamic_entries + (sizeof(dynamic_trustcache_entry_v0_t) * i), &entry, sizeof(dynamic_trustcache_entry_v0_t));
            
            if (memcmp(&entry.cd_hash[0], cd_hash, 20) == 0) {
                memset(&entry.cd_hash[0], 0x41, 20);
            }
        }
    }
}

int trustcache_add_binary(const char *path) {
    if (tc_info == NULL || tc_info->dynamic_entries == 0 || path == NULL) return -1;
    macho_ctx_t *macho = macho_load(path);
    if (macho == NULL) return -1;
    
    macho_rpaths_t *rpaths = macho_resolve_rpaths(macho);
    if (rpaths == NULL) {
        macho_release(macho);
        return -1;
    }
    
    macho_deps_t *deps = macho_resolve_deps(macho, rpaths);
    macho_release_rpaths(rpaths);
    macho_release(macho);
    if (deps == NULL) return -1;
    
    int added = 0;
    for (uint32_t i = 0; i < deps->count; i++) {
        macho_ctx_t *current_macho = macho_load(deps->list[i]);
        if (current_macho == NULL) continue;
        
        for (uint32_t j = 0; j < current_macho->slice_count; j++) {
            macho_signature_t *signature = macho_get_signature(&current_macho->slice_list[j]);
            if (signature == NULL) continue;
            
            if (!trustcache_check(signature->hash)) {
                trustcache_add_hash(signature->hash, signature->hash_type);
                added++;
            }
            macho_release_signature(signature);
        }
        macho_release(current_macho);
    }
    return 0;
}
