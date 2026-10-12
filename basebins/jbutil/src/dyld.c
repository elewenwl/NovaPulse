#include "util.h"
#include "dyld.h"

#define DYLD_PATCH_INFO_OFFSET_12   0x928
#define FCNTL_HOOK_OFFSET_12        0x5b4
#define MMAP_HOOK_OFFSET_12         0x708

#define DYLD_PATCH_INFO_OFFSET_13   0xa0c
#define FCNTL_HOOK_OFFSET_13        0x698
#define MMAP_HOOK_OFFSET_13         0x7ec

#define DYLD_PATCH_INFO_OFFSET      ((version[0] <= 12) ? DYLD_PATCH_INFO_OFFSET_12 : DYLD_PATCH_INFO_OFFSET_13)
#define FCNTL_HOOK_OFFSET           ((version[0] <= 12) ? FCNTL_HOOK_OFFSET_12 : FCNTL_HOOK_OFFSET_13)
#define MMAP_HOOK_OFFSET            ((version[0] <= 12) ? MMAP_HOOK_OFFSET_12 : MMAP_HOOK_OFFSET_13)

static dyld_info_t *dyld_info = NULL;
static uint32_t version[3] = {0};
typedef struct section_64 section_t;
typedef struct segment_command_64 segment_command_t;
typedef struct mach_header_64 mach_header_t;
typedef struct nlist_64 nlist_t;

static dyld_cache_ranged_mapping_t *dyld_cache_map_range(int fd, uint32_t offset, uint32_t size) {
    dyld_cache_ranged_mapping_t *mapping = calloc(1, sizeof(dyld_cache_ranged_mapping_t));
    if (mapping == NULL) return NULL;

    uint32_t map_offset = (uint32_t)(trunc_page(offset));
    mapping->aligned_size = (uint32_t)(round_page(size));
    mapping->size = size;

    lseek(fd, 0, SEEK_SET);
    mapping->aligned_data = mmap(NULL, mapping->aligned_size, PROT_READ, MAP_PRIVATE, fd, map_offset);
    if (mapping->aligned_data == MAP_FAILED) {
        free(mapping);
        return NULL;
    }

    mapping->data = (void *)((uintptr_t)mapping->aligned_data + (offset - map_offset));
    return mapping;
}

static void dyld_cache_unmap_range(dyld_cache_ranged_mapping_t *mapping) {
    if (mapping == NULL) return;
    if (mapping->aligned_data != NULL) {
        munmap(mapping->aligned_data, mapping->aligned_size);
    }
    free(mapping);
}

static int32_t dyld_cache_image_index(const char *path) {
    if (dyld_info == NULL || path == NULL) return -1;
    uint32_t image_count = 0;
    uint32_t images_offset = 0;

    if (dyld_info->cache_hdr->mappingOffset >= offsetof(dyld_cache_header_t, imagesOffset)) {
        images_offset = dyld_info->cache_hdr->imagesOffset;
        image_count = dyld_info->cache_hdr->imagesCount;
    } else {
        images_offset = dyld_info->cache_hdr->imagesOffsetLegacy;
        image_count = dyld_info->cache_hdr->imagesCountLegacy;
    }

    if (images_offset == 0 || image_count == 0) return -1;
    dyld_cache_image_info_t *image_list = (dyld_cache_image_info_t *)((uint8_t *)dyld_info->cache_hdr + images_offset);

    for (int32_t idx = 0; idx < image_count; idx++) {
        char *image_path = ((char *)dyld_info->cache_hdr) + image_list[idx].pathFileOffset;
        if (strcmp(image_path, path) == 0) {
            return idx;
        }
    }
    return -1;
}

static void *dyld_cache_find_symbol(const char *path, const char *name, bool no_slide) {
    uint32_t image_idx = dyld_cache_image_index(path);
    if (image_idx == -1) return NULL;

    nlist_t *symbol_table = (nlist_t *)dyld_info->symbol_mapping->data;
    char *string_table = (char *)dyld_info->string_mapping->data;
    uint32_t start_idx = 0;
    uint32_t end_idx = 0;

    if (dyld_info->legacy_entries) {
        dyld_cache_local_symbols_entry_t *symbols_entry = (dyld_cache_local_symbols_entry_t *)dyld_info->entry_mapping->data;
        start_idx = symbols_entry[image_idx].nlistStartIndex;
        end_idx = start_idx + symbols_entry[image_idx].nlistCount;
    } else {
        dyld_cache_local_symbols_entry_64_t *symbols_entry = (dyld_cache_local_symbols_entry_64_t *)dyld_info->entry_mapping->data;
        start_idx = symbols_entry[image_idx].nlistStartIndex;
        end_idx = start_idx + symbols_entry[image_idx].nlistCount;
    }

    if (end_idx > dyld_info->symbol_info.nlistCount) return NULL;
    for (uint32_t i = start_idx; i < end_idx; i++) {
        if (strcmp(name, string_table + symbol_table[i].n_un.n_strx) == 0) {
            if (no_slide) return (void *)(symbol_table[i].n_value);
            return (void *)(symbol_table[i].n_value + dyld_info->cache_slide);
        }
    }
    return NULL;
}

static void dyld_deinit(void) {
    if (dyld_info == NULL) return;
    if (dyld_info->entry_mapping != NULL) dyld_cache_unmap_range(dyld_info->entry_mapping);
    if (dyld_info->symbol_mapping != NULL) dyld_cache_unmap_range(dyld_info->symbol_mapping);
    if (dyld_info->string_mapping != NULL) dyld_cache_unmap_range(dyld_info->string_mapping);
    
    free(dyld_info);
    dyld_info = NULL;
}

static int dyld_init(void) {
    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;
    int fd = -1;

    if ((dyld_info = calloc(1, sizeof(dyld_info_t))) == NULL) goto err;
    task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&info, &count);
    dyld_all_image_infos_t *all_image_infos = (dyld_all_image_infos_t *)info.all_image_info_addr;
    if (all_image_infos == NULL) goto err;

    dyld_info->cache_hdr = (dyld_cache_header_t *)all_image_infos->sharedCacheBaseAddress;
    dyld_info->cache_slide = (uintptr_t)all_image_infos->sharedCacheSlide;
    dyld_info->dyld_base = (uintptr_t)all_image_infos->dyldImageLoadAddress;

    if ((fd = open(dyld_shared_cache_file_path(), O_RDONLY)) < 0) goto err;
    dyld_cache_header_t hdr = {};

    read(fd, &hdr, sizeof(dyld_cache_header_t));
    if (hdr.localSymbolsOffset == 0 || hdr.localSymbolsSize == 0) goto err;

    dyld_info->legacy_entries = !(hdr.mappingOffset >= offsetof(dyld_cache_header_t, symbolFileUUID));
    lseek(fd, hdr.localSymbolsOffset, SEEK_SET);
    read(fd, &dyld_info->symbol_info, sizeof(dyld_cache_local_symbols_info_t));
    
    uint32_t entries_offset = (uint32_t)(hdr.localSymbolsOffset + dyld_info->symbol_info.entriesOffset);
    uint32_t entries_size = 0;
    if (dyld_info->legacy_entries) {
        entries_size = (dyld_info->symbol_info.entriesCount * sizeof(dyld_cache_local_symbols_entry_t));
    } else {
        entries_size = (dyld_info->symbol_info.entriesCount * sizeof(dyld_cache_local_symbols_entry_64_t));
    }

    if ((dyld_info->entry_mapping = dyld_cache_map_range(fd, entries_offset, entries_size)) == NULL) goto err;
    uint32_t symbol_offset = (uint32_t)(hdr.localSymbolsOffset + dyld_info->symbol_info.nlistOffset);
    uint32_t symbol_size = dyld_info->symbol_info.nlistCount * sizeof(nlist_t);

    if ((dyld_info->symbol_mapping = dyld_cache_map_range(fd, symbol_offset, symbol_size)) == NULL) goto err;
    uint32_t string_offset = (uint32_t)(hdr.localSymbolsOffset + dyld_info->symbol_info.stringsOffset);
    uint32_t string_size = dyld_info->symbol_info.stringsSize;

    if ((dyld_info->string_mapping = dyld_cache_map_range(fd, string_offset, string_size)) == NULL) goto err;
    close(fd);
    return 0;

err:
    if (fd >= 0) close(fd);
    dyld_deinit();
    return -1;
}

static void *find_dyld_symbol(const char *name, dyld_slice_t *slice) {
    struct load_command *load_cmd = slice->load_cmd;
    struct symtab_command *sym_cmd = NULL;
    uint64_t stroff = 0;
    uint64_t symoff = 0;

    for (int i = 0; i < slice->cmd_count; i++) {
        if (load_cmd->cmd == LC_SYMTAB) sym_cmd = (struct symtab_command *)load_cmd;
        load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
    }

    load_cmd = slice->load_cmd;
    for (int i = 0; i < slice->cmd_count; i++) {
        if (load_cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
            if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                if ((sym_cmd->symoff - segment->fileoff) < segment->filesize) symoff = sym_cmd->symoff;
                if ((sym_cmd->stroff - segment->fileoff) < segment->filesize) stroff = sym_cmd->stroff;
            }
            if (stroff != 0 && symoff != 0) break;
        }
        load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
    }

    struct nlist_64 *symbol = (struct nlist_64 *)(slice->hdr + symoff);
    for (int i = 0; i < sym_cmd->nsyms; i++ && symbol++) {
        uint64_t str_idx = symbol->n_un.n_strx;

        if (str_idx >= sym_cmd->strsize || str_idx == 0) continue;
        if (symbol->n_type == 115 || symbol->n_type == 17) continue;
        if (strcmp((const char *)(slice->hdr + stroff + str_idx), name) == 0) {
            if (symbol->n_value == 0) continue;
            return (void *)(slice->hdr + symbol->n_value);
        }
    }
    return NULL;
}

static uint64_t find_dyld_symbol_va(const char *name, dyld_slice_t *slice) {
    void *local_addr = find_dyld_symbol(name, slice);
    if (local_addr == NULL) return 0;
    return ((uint64_t)local_addr - (uint64_t)slice->hdr);
}

static void deinit_dyld_patch(dyld_patch_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (ctx->file_data != NULL) munmap(ctx->file_data, ctx->file_size);
    free(ctx);
    return;
}

static dyld_patch_ctx_t *init_dyld_patch(void) {
    int fd = open("/usr/lib/dyld", O_RDONLY);
    if (fd < 0) return NULL;

    dyld_patch_ctx_t *ctx = calloc(1, sizeof(dyld_patch_ctx_t));
    ctx->file_size = (uint32_t)lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    ctx->file_data = mmap(NULL, ctx->file_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (ctx->file_data == MAP_FAILED) goto err;
    
    if (MACHO_FAT(*(uint32_t *)ctx->file_data)) {
        bool need_swap = MACHO_REQUIRES_SWAP(*(uint32_t *)ctx->file_data);
        struct fat_header fhdr = {0};
        memcpy(&fhdr, (struct fat_header *)ctx->file_data, sizeof(struct fat_header));
        if (need_swap) swap_fat_header(&fhdr, 0);
        
        for (int i = 0; i < fhdr.nfat_arch; i++) {
            size_t arch_offset = sizeof(struct fat_header) + (sizeof(struct fat_arch) * i);
            struct fat_arch arch = {0};
            memcpy(&arch, ctx->file_data + arch_offset, sizeof(struct fat_arch));
            if (need_swap) swap_fat_arch(&arch, fhdr.nfat_arch, 0);

            if (arch.cputype == CPU_TYPE_ARM64) {
                if (arch.cpusubtype == CPU_SUBTYPE_ARM64E) {
                    ctx->arm64e.offset = arch.offset;
                    ctx->arm64e.size = arch.size;
                } else {
                    ctx->arm64.offset = arch.offset;
                    ctx->arm64.size = arch.size;
                }
            }
        }
    } else {
        if (soc_is_arm64e()) {
            ctx->arm64e.offset = 0;
            ctx->arm64e.size = ctx->file_size;
        } else {
            ctx->arm64.offset = 0;
            ctx->arm64.size = ctx->file_size;
        }
    }
    
    if (ctx->arm64e.size > 0) {
        ctx->arm64e.hdr = ctx->file_data + ctx->arm64e.offset;
        ctx->arm64e.load_cmd = (struct load_command *)(ctx->arm64e.hdr64 + 1);
        ctx->arm64e.cmd_count = ctx->arm64e.hdr64->ncmds;
        ctx->arm64e.has_pac = true;

        struct load_command *load_cmd = ctx->arm64e.load_cmd;
        for (int i = 0; i < ctx->arm64e.cmd_count; i++) {
            if (load_cmd->cmd == LC_SEGMENT_64) {
                struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
                if (strcmp(segment->segname, "__TEXT") == 0) {
                    struct section_64 *section = (struct section_64 *)(segment + 1);

                    for (uint32_t j = 0; j < segment->nsects; j++) {
                        if (strcmp(section[j].sectname, "__text") == 0) {
                            ctx->arm64e.text_data = (uint8_t *)ctx->arm64e.hdr + section[j].offset;
                            ctx->arm64e.text_size = (uint32_t)section[j].size;
                            break;
                        }
                    }
                    if (ctx->arm64e.text_data != NULL) break;
                }
            }
            load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
        }
    }
    
    if (ctx->arm64.size > 0) {
        ctx->arm64.hdr = ctx->file_data + ctx->arm64.offset;
        ctx->arm64.load_cmd = (struct load_command *)(ctx->arm64.hdr64 + 1);
        ctx->arm64.cmd_count = ctx->arm64.hdr64->ncmds;
        ctx->arm64.has_pac = false;

        struct load_command *load_cmd = ctx->arm64.load_cmd;
        for (int i = 0; i < ctx->arm64.cmd_count; i++) {
            if (load_cmd->cmd == LC_SEGMENT_64) {
                struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
                if (strcmp(segment->segname, "__TEXT") == 0) {
                    struct section_64 *section = (struct section_64 *)(segment + 1);

                    for (uint32_t j = 0; j < segment->nsects; j++) {
                        if (strcmp(section[j].sectname, "__text") == 0) {
                            ctx->arm64.text_data = (uint8_t *)ctx->arm64.hdr + section[j].offset;
                            ctx->arm64.text_size = (uint32_t)section[j].size;
                            break;
                        }
                    }
                    if (ctx->arm64.text_data != NULL) break;
                }
            }
            load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
        }
    }
    return ctx;

err:
    deinit_dyld_patch(ctx);
    return NULL;
}

static void dyld_hook_function(void *from, void *to) {
    int32_t diff = (from <= to) ? (int32_t)(to - from) : (int32_t)(-(from - to));
    uint32_t inst = ((diff >> 2) & 0x3FFFFFF) | 0x14000000;
    *(uint32_t *)from = inst;
}

static uint8_t *bh_memmem(const uint8_t* haystack, size_t hlen, const uint8_t* needle, size_t nlen) {
    size_t last, scan = 0;
    size_t skip[256];
    
    if (nlen <= 0 || !haystack || !needle) return NULL;
    for (scan = 0; scan <= 255; scan = scan + 1) skip[scan] = nlen;

    last = nlen - 1;
    for (scan = 0; scan < last; scan = scan + 1) skip[needle[scan]] = last - scan;

    while (hlen >= nlen) {
        for (scan = last; haystack[scan] == needle[scan]; scan = scan - 1)
            if (scan == 0) return (void *)haystack;

        hlen -= skip[haystack[last]];
        haystack += skip[haystack[last]];
    }
    return NULL;
}

static int patch_amfi_flags(dyld_slice_t *slice) {
    void *symbol = find_dyld_symbol("_amfi_check_dyld_policy_self", slice);
    if (symbol == NULL) {
        uint32_t bytes[4] = {
            0xb40002e1, // cbz x1, 0x78
            0xd10103ff, // sub sp, sp, #0x40
            0xa9024ff4, // stp x20, x19, [sp, #0x20]
            0xa9037bfd  // stp x29, x30, [sp, #0x30]
        };
        
        symbol = (void *)bh_memmem(slice->text_data, slice->text_size, (uint8_t *)bytes, sizeof(bytes));
        if (symbol == NULL) {
            uint32_t bytes[6] = {
                0xd10103ff, // sub sp, sp, #0x40
                0xa9024ff4, // stp x20, x19, [sp, #0x20]
                0xa9037bfd, // stp x29, x30, [sp, #0x30]
                0x9100c3fd, // add x29, sp, #0x30
                0xaa0103f3, // mov x19, x1
                0xb40001f3  // cbz x19, 0x50
            };
            symbol = (void *)bh_memmem(slice->text_data, slice->text_size, (uint8_t *)bytes, sizeof(bytes));
            if (symbol == NULL) return -1;
        }
    }
    
    uint32_t *inst = (uint32_t *)symbol;
    inst[0] = 0xd28007e8; // mov x8, #0x3f
    inst[1] = 0xf9000028; // str x8, [x1]
    inst[2] = 0xaa1f03e0; // mov x0, xzr
    inst[3] = 0xd65f03c0; // ret
    return 0;
}

static int patch_closures(dyld_slice_t *slice) {
    void *symbol = find_dyld_symbol("__ZN5dyld315internalInstallEv", slice);
    if (symbol == NULL) {
        uint32_t bytes[5] = {
            0xb2704fe8, // mov x8, #0xfffff0000
            0xf2981088, // movk x8, #0xc084
            0xb9400108, // ldr w8, [x8]
            0x12000100, // and w0, w8, #1
            0xd65f03c0  // ret
        };
        
        symbol = (void *)bh_memmem(slice->text_data, slice->text_size, (uint8_t *)bytes, sizeof(bytes));
        if (symbol == NULL) return -1;
    }
    
    uint32_t *inst = (uint32_t *)symbol;
    inst[0] = 0xaa1f03e0; // mov x0, xzr
    inst[1] = 0xd65f03c0; // ret
    
    symbol = find_dyld_symbol("__ZN4dyldL12closureValidEPKN5dyld37closure13LaunchClosureERKNS1_14LoadedFileInfoEPKhbPPKc", slice);
    if (symbol == NULL) return -1;
    
    inst = (uint32_t *)symbol;
    inst[0] = 0xaa1f03e0; // mov x0, xzr
    inst[1] = 0xd65f03c0; // ret
    return 0;
}

static int patch_eh_frame(dyld_slice_t *slice) {
    struct load_command *load_cmd = slice->load_cmd;
    struct segment_command_64 *text_segment = NULL;
    struct section_64 *eh_frame_section = NULL;
    
    for (int i = 0; i < slice->cmd_count; i++) {
        if (load_cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *)load_cmd;
            if (strcmp(segment->segname, "__TEXT") == 0) {
                struct section_64 *section = (struct section_64 *)(segment + 1);

                for (uint32_t j = 0; j < segment->nsects; j++) {
                    if (strcmp(section[j].sectname, "__eh_frame") == 0) {
                        text_segment = segment;
                        eh_frame_section = &section[j];
                        break;
                    }
                }
                if (eh_frame_section != NULL) break;
            }
        }
        load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
    }
    
    if (eh_frame_section == NULL) return -1;
    uint32_t leftover_start = ((((sizeof(struct mach_header_64) + slice->hdr64->sizeofcmds + 0x10)) + (32-1)) & ~(32-1));
    uint32_t leftover_size = 0x1000 - leftover_start;
    if (leftover_size < sizeof(dyld_patch_sb_ext_t)) return -1;
    
    dyld_patch_sb_ext_t *sb_exts = (dyld_patch_sb_ext_t *)(slice->hdr + leftover_start);
    bzero(sb_exts, sizeof(dyld_patch_sb_ext_t));
    strcpy(sb_exts->sb_ext1_str, sandbox_extension_issue_file(SANDBOX_READ, "/private/var/mobile", 0));
    strcpy(sb_exts->sb_ext2_str, sandbox_extension_issue_file(SANDBOX_READ_WRITE, "/private/var/mobile/Library/Preferences", 0));
    strcpy(sb_exts->sb_ext3_str, sandbox_extension_issue_file(SANDBOX_READ_WRITE, "/private/var/cache", 0));
    strcpy(sb_exts->sb_ext4_str, sandbox_extension_issue_file(SANDBOX_READ_WRITE, "/Library", 0));
    strcpy(sb_exts->sb_ext5_str, sandbox_extension_issue_file(SANDBOX_EXECUTABLE, "/Library", 0));
    strcpy(sb_exts->sb_ext6_str, sandbox_extension_issue_file(SANDBOX_EXECUTABLE, "/private/var", 0));
    sb_exts->___padding = 0;
    
    uint32_t text_size = (uint32_t)text_segment->filesize;
    uint32_t orig_eh_frame_offset = eh_frame_section->offset;
    uint32_t orig_eh_frame_size = (uint32_t)eh_frame_section->size;

    uint32_t eh_frame_offset = orig_eh_frame_offset + orig_eh_frame_size;
    uint32_t eh_frame_size = text_size - eh_frame_offset;
    eh_frame_section->size = text_size - orig_eh_frame_offset;
    
    uint32_t shc_size = 0;
    uint8_t *shc_data = NULL;
    
    if (version[0] <= 12) {
        shc_data = map_file("/amethyst/dyld_patch_text_12.bin", &shc_size, true);
    } else {
        shc_data = map_file("/amethyst/dyld_patch_text_13.bin", &shc_size, true);
    }
    
    if (shc_data == NULL) return -1;
    if (shc_size > eh_frame_size) {
        munmap(shc_data, shc_size);
        return -1;
    }
    
    uint64_t info_va = (uint64_t)eh_frame_offset + DYLD_PATCH_INFO_OFFSET;
    dyld_patch_info_t *info = (dyld_patch_info_t *)(shc_data + DYLD_PATCH_INFO_OFFSET);
    info->mach_init_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, mach_init_symbol)) - find_dyld_symbol_va("_mach_init", slice));
    info->getenv_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, getenv_symbol)) - find_dyld_symbol_va("__simple_getenv", slice));
    info->strstr_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, strstr_symbol)) - find_dyld_symbol_va("_strstr", slice));
    info->sb_ext_offset = (uint32_t)((info_va + offsetof(dyld_patch_info_t, sb_ext_offset)) - leftover_start);
    info->dyld_start_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, dyld_start_symbol)) - 0x1008);
    info->mach_task_self_symbol = (uint32_t)(find_dyld_symbol_va("_mach_task_self_", slice) - (info_va + offsetof(dyld_patch_info_t, mach_task_self_symbol)));
    info->errno_symbol = (uint32_t)(find_dyld_symbol_va("_errno", slice) - (info_va + offsetof(dyld_patch_info_t, errno_symbol)));
    
    if (version[0] >= 13) {
        info->cc_sha1_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha1_symbol)) - find_dyld_symbol_va("_ccsha1_di", slice));
        info->cc_sha256_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha256_symbol)) - find_dyld_symbol_va("_ccsha256_di", slice));
        info->cc_sha384_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha384_symbol)) - find_dyld_symbol_va("_ccsha384_di", slice));
        info->ccdigest_init_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, ccdigest_init_symbol)) - find_dyld_symbol_va("_ccdigest_init", slice));
        info->ccdigest_update_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, ccdigest_update_symbol)) - find_dyld_symbol_va("_ccdigest_update", slice));
        info->cc_clear_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_clear_symbol)) - find_dyld_symbol_va("_cc_clear", slice));
    } else {
        info->cc_sha1_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha1_symbol)) - find_dyld_symbol_va("_CC_SHA1", slice));
        info->cc_sha256_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha256_symbol)) - find_dyld_symbol_va("_CC_SHA256", slice));
        info->cc_sha384_symbol = (uint32_t)((info_va + offsetof(dyld_patch_info_t, cc_sha384_symbol)) - find_dyld_symbol_va("_CC_SHA384", slice));
        info->ccdigest_init_symbol = 0;
        info->ccdigest_update_symbol = 0;
        info->cc_clear_symbol = 0;
    }

    strcpy(info->dyld_insert_str, "DYLD_INSERT_LIBRARIES");
    strcpy(info->base_hook_str, "/usr/lib/base_hook.dylib");
    strcpy(info->sb_policy_str, "Sandbox");

    info->arm64e_host = soc_is_arm64e() ? 1 : 0;
    info->page_size = get_page_size();
    info->ios_version[0] = version[0];
    info->ios_version[1] = version[1];
    info->ios_version[2] = version[2];
    info->ns_init_platform_symbol = (uint64_t)dyld_cache_find_symbol("/System/Library/Frameworks/Foundation.framework/Foundation", "__NSInitializePlatform", true);
    info->use_dyld3_symbol = (uint64_t)dyld_cache_find_symbol("/usr/lib/system/libdyld.dylib", "_gUseDyld3", true);
    
    bzero(slice->hdr + eh_frame_offset, eh_frame_size);
    memcpy(slice->hdr + eh_frame_offset, shc_data, shc_size);
    dyld_hook_function(slice->hdr + 0x1000, slice->hdr + eh_frame_offset);
    *(uint32_t *)(slice->hdr + 0x1004) = (uint32_t)info_va;
    
    dyld_hook_function(find_dyld_symbol("___fcntl", slice), slice->hdr + eh_frame_offset + FCNTL_HOOK_OFFSET);
    dyld_hook_function(find_dyld_symbol("_xmmap", slice), slice->hdr + eh_frame_offset + MMAP_HOOK_OFFSET);
    munmap(shc_data, shc_size);
    return 0;
}

static uint32_t get_hash_rank(const CS_CodeDirectory *cd) {
    switch (cd->hashType) {
        case CS_HASHTYPE_SHA1: return 1;
        case CS_HASHTYPE_SHA256_TRUNCATED: return 2;
        case CS_HASHTYPE_SHA256: return 3;
        case CS_HASHTYPE_SHA384: return 4;
        default: return 0;
    }
}

static uint8_t *get_cd_hash(CS_CodeDirectory *cd) {
    uint32_t size = ntohl(cd->length);
    uint8_t *hash = calloc(1, CS_CDHASH_LEN);
    
    switch (cd->hashType) {
        case CS_HASHTYPE_SHA1: CC_SHA1(cd, size, hash); break;
        case CS_HASHTYPE_SHA256:
        case CS_HASHTYPE_SHA256_TRUNCATED: CC_SHA256(cd, size, hash); break;
        case CS_HASHTYPE_SHA384: CC_SHA384(cd, size, hash); break;
        default: return NULL;
    }
    return hash;
}

static int resign_dyld(dyld_slice_t *slice) {
    struct load_command *load_cmd = slice->load_cmd;
    struct linkedit_data_command *linkedit = NULL;

    for (int i = 0; i < slice->cmd_count; i++) {
        if (load_cmd->cmd == LC_CODE_SIGNATURE) {
            linkedit = (struct linkedit_data_command *)load_cmd;
            break;
        }
        load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
    }
    
    if (linkedit == NULL) return -1;
    CS_SuperBlob *super_blob = (CS_SuperBlob *)(slice->hdr + linkedit->dataoff);
    uint32_t best_rank = 0;
    CS_CodeDirectory *best_cd = NULL;
    
    for (int i = 0; i < ntohl(super_blob->count); i++) {
        const CS_BlobIndex *index = &super_blob->index[i];
        uint32_t type = ntohl(index->type);
        uint32_t offset = ntohl(index->offset);
        if (ntohl(super_blob->length) < offset) break;
        
        CS_CodeDirectory *sub = (CS_CodeDirectory *)((uint64_t)super_blob + offset);
        if (type == CSSLOT_CODEDIRECTORY ||
            (type >= CSSLOT_ALTERNATE_CODEDIRECTORIES &&
             type < CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT)) {
            
            uint32_t rank = get_hash_rank(sub);
            if (rank > best_rank) {
                best_rank = rank;
                best_cd = sub;
            }
        }
    }
    
    if (best_cd == NULL) return -1;
    int slot_count = (int)ntohl(best_cd->nCodeSlots);
    uint32_t page_size = (uint32_t)(pow(2.0, (double)(best_cd->pageSize)));
    
    for (int i = 0; i < slot_count; i++) {
        uint8_t *expected_hash = (uint8_t *)best_cd + ntohl(best_cd->hashOffset) + (i * best_cd->hashSize);
        uint8_t real_hash[best_cd->hashSize];
        bzero(real_hash, best_cd->hashSize);
        
        uint64_t page_offset = (uint64_t)(i * page_size);
        uint8_t *page = (uint8_t *)(slice->hdr + page_offset);
        uint32_t calc_size = page_size;
        if (i == ntohl(best_cd->nCodeSlots) - 1) {
            calc_size = ntohl(best_cd->codeLimit) - (uint32_t)page_offset;
        }
        
        switch (best_cd->hashType) {
            case CS_HASHTYPE_SHA1: CC_SHA1(page, calc_size, real_hash); break;
            case CS_HASHTYPE_SHA256:
            case CS_HASHTYPE_SHA256_TRUNCATED: CC_SHA256(page, calc_size, real_hash); break;
            case CS_HASHTYPE_SHA384: CC_SHA384(page, calc_size, real_hash); break;
            default: break;
        }
        
        if (memcmp(expected_hash, real_hash, best_cd->hashSize) != 0) {
            memcpy(expected_hash, real_hash, best_cd->hashSize);
        }
    }
    return 0;
}

int patch_dyld(void) {
    char test_path[PATH_MAX] = {0};
    realpath("/usr/lib/dyld", test_path);
    if (strcmp(test_path, "/usr/lib/dyld_patch") == 0) return 0;
    get_ios_version(&version[0]);

    dyld_init();
    dyld_patch_ctx_t *ctx = init_dyld_patch();
    if (ctx->arm64.size > 0) {
        patch_amfi_flags(&ctx->arm64);
        patch_eh_frame(&ctx->arm64);
        patch_closures(&ctx->arm64);
        resign_dyld(&ctx->arm64);
    }

    if (ctx->arm64e.size > 0) {
        patch_amfi_flags(&ctx->arm64e);
        patch_eh_frame(&ctx->arm64e);
        patch_closures(&ctx->arm64e);
        resign_dyld(&ctx->arm64e);
    }

    dyld_deinit();
    FILE *file = fopen("/amethyst/dyld_patch", "wb+");
    fwrite((void *)ctx->file_data, ctx->file_size, 1, file);
    fflush(file);
    fclose(file);
    sync();

    copy_file("/amethyst/dyld_patch", "/usr/lib/dyld_patch");
    chmod("/usr/lib/dyld_patch", 0755);
    chown("/usr/lib/dyld_patch", 0, 0);
    return 0;
}
