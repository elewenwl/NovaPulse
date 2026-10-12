#include "basebin_util.h"
#include "basebin_macho.h"

#ifdef LAUNCHD_HOOK
#include <CoreFoundation/CoreFoundation.h>
#endif

void macho_release(macho_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (ctx->slice_list != NULL) free(ctx->slice_list);
    if (ctx->file_data != NULL) munmap(ctx->file_data, ctx->file_size);
    if (ctx->path != NULL) free(ctx->path);
    free(ctx);
}

static bool macho_supported(cpu_type_t cpu_type, cpu_subtype_t cpu_subtype, uint32_t file_type) {
    if (cpu_type != CPU_TYPE_ARM64) return false;
    if (cpu_subtype == CPU_SUBTYPE_ARM64_V8 || cpu_subtype == CPU_SUBTYPE_ARM64_ALL) return true;
    
    if (basebin_info.arm64e_host) {
        if (cpu_subtype == CPU_SUBTYPE_ARM64E) return true;
        if (cpu_subtype == (CPU_SUBTYPE_ARM64E | CPU_SUBTYPE_PTRAUTH_ABI)) {
            if (file_type == MH_DYLIB || file_type == MH_BUNDLE) return true;
        }
    }
    return false;
}

macho_ctx_t *macho_load(const char *path) {
    if (path == NULL || path[0] == '\0') return NULL;
    macho_ctx_t *ctx = NULL;
    int fd = -1;
    
    if ((fd = open(path, O_RDONLY)) < 0) goto err;
    if ((ctx = calloc(1, sizeof(macho_ctx_t))) == NULL) goto err;
    if ((ctx->file_size = (uint32_t)lseek(fd, 0, SEEK_END)) < sizeof(struct mach_header)) goto err;
    lseek(fd, 0, SEEK_SET);
    
    if ((ctx->file_data = mmap(NULL, ctx->file_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) goto err;
    close(fd);
    fd = -1;
    
    if ((ctx->path = strdup(path)) == NULL) goto err;
    uint32_t magic = *(uint32_t *)ctx->file_data;
    if (!MACHO_VALID(magic)) goto err;
    bool need_swap = MACHO_REQUIRES_SWAP(magic);
    
    if (MACHO_FAT(magic)) {
        struct fat_header fhdr = {0};
        memcpy(&fhdr, ctx->file_data, sizeof(struct fat_header));
        if (need_swap) swap_fat_header(&fhdr, 0);

        bool large_slices = (fhdr.magic == FAT_MAGIC_64 || fhdr.magic == FAT_CIGAM_64);
        ctx->slice_list = calloc(1, sizeof(macho_slice_t) * (fhdr.nfat_arch + 1));
        if (ctx->slice_list == NULL) goto err;
        ctx->slice_count = 0;

        for (uint32_t i = 0; i < fhdr.nfat_arch; i++) {
            macho_arch_t arch = {0};
            if (large_slices) {
                struct fat_arch_64 arch64 = {0};
                memcpy(&arch64, ctx->file_data + (sizeof(struct fat_header) + (sizeof(struct fat_arch_64) * i)), sizeof(struct fat_arch_64));
                if (need_swap) swap_fat_arch_64(&arch64, 1, 0);
                arch.cpu_type = arch64.cputype;
                arch.cpu_subtype = arch64.cpusubtype;
                arch.offset = (uint32_t)arch64.offset;
                arch.size = (uint32_t)arch64.size;
                arch.alignment = arch64.align;
            } else {
                struct fat_arch arch32 = {0};
                memcpy(&arch32, ctx->file_data + (sizeof(struct fat_header) + (sizeof(struct fat_arch) * i)), sizeof(struct fat_arch));
                if (need_swap) swap_fat_arch(&arch32, 1, 0);
                arch.cpu_type = arch32.cputype;
                arch.cpu_subtype = arch32.cpusubtype;
                arch.offset = arch32.offset;
                arch.size = arch32.size;
                arch.alignment = arch32.align;
            }
          
            if (MACHO_32BIT(*(uint32_t *)(ctx->file_data + arch.offset))) continue;
            if ((arch.offset & 0xfff) != 0 || arch.size < 0x1000) continue;

            struct mach_header_64 hdr = {0};
            memcpy(&hdr, ctx->file_data + arch.offset, sizeof(struct mach_header_64));
            if (MACHO_REQUIRES_SWAP(hdr.magic)) swap_mach_header_64(&hdr, 0);      
            
            if (macho_supported(arch.cpu_type, arch.cpu_subtype, hdr.filetype)) {
                ctx->slice_list[ctx->slice_count].hdr = (struct mach_header_64 *)(ctx->file_data + arch.offset);
                ctx->slice_list[ctx->slice_count].load_cmd = (struct load_command *)(ctx->slice_list[ctx->slice_count].hdr + 1);
                ctx->slice_list[ctx->slice_count].cpu_type = arch.cpu_type;
                ctx->slice_list[ctx->slice_count].cpu_subtype = arch.cpu_subtype;
                ctx->slice_list[ctx->slice_count].file_type = hdr.filetype;
                ctx->slice_list[ctx->slice_count].cmd_count = hdr.ncmds;
                ctx->slice_list[ctx->slice_count].offset = arch.offset;
                ctx->slice_list[ctx->slice_count].size = arch.size;
                ctx->slice_list[ctx->slice_count].file_data = ctx->file_data;
                ctx->slice_list[ctx->slice_count].file_size = ctx->file_size;
                ctx->slice_list[ctx->slice_count].has_ptrauth = ((arch.cpu_type == CPU_TYPE_ARM64) && (arch.cpu_subtype & CPU_SUBTYPE_ARM64E) != 0);
                ctx->slice_list[ctx->slice_count].path = ctx->path;
                ctx->slice_count++;
            }
        }
        if (ctx->slice_count == 0) goto err;
    } else {
        ctx->slice_list = calloc(1, sizeof(macho_slice_t) * 2);
        if (ctx->slice_list == NULL) goto err;
        ctx->slice_count = 0;

        if (MACHO_32BIT(*(uint32_t *)(ctx->file_data))) goto err;
        struct mach_header_64 hdr = {0};
        memcpy(&hdr, ctx->file_data, sizeof(struct mach_header_64));
        if (MACHO_REQUIRES_SWAP(hdr.magic)) swap_mach_header_64(&hdr, 0);      
        
        if (macho_supported(hdr.cputype, hdr.cpusubtype, hdr.filetype)) {
            ctx->slice_list[ctx->slice_count].hdr = (struct mach_header_64 *)(ctx->file_data);
            ctx->slice_list[ctx->slice_count].load_cmd = (struct load_command *)(ctx->slice_list[ctx->slice_count].hdr + 1);
            ctx->slice_list[ctx->slice_count].cpu_type = hdr.cputype;
            ctx->slice_list[ctx->slice_count].cpu_subtype = hdr.cpusubtype;
            ctx->slice_list[ctx->slice_count].file_type = hdr.filetype;
            ctx->slice_list[ctx->slice_count].cmd_count = hdr.ncmds;
            ctx->slice_list[ctx->slice_count].offset = 0;
            ctx->slice_list[ctx->slice_count].size = ctx->file_size;
            ctx->slice_list[ctx->slice_count].file_data = ctx->file_data;
            ctx->slice_list[ctx->slice_count].file_size = ctx->file_size;
            ctx->slice_list[ctx->slice_count].has_ptrauth = ((hdr.cputype == CPU_TYPE_ARM64) && (hdr.cpusubtype & CPU_SUBTYPE_ARM64E) != 0);
            ctx->slice_list[ctx->slice_count].path = ctx->path;
            ctx->slice_count++;
        }
    }
    
    if (ctx->slice_count == 0) goto err;
    return ctx;

err:
    if (fd >= 0) close(fd);
    macho_release(ctx);
    return NULL;
}

int macho_get_entitlements(macho_slice_t *slice, macho_signature_t *signature, uint8_t **out_data, uint32_t *out_size) {
#ifdef LAUNCHD_HOOK
    if (slice == NULL || signature == NULL || out_data == NULL || out_size == NULL) return -1;
    uint8_t *xml_data = NULL;
    uint32_t xml_size = 0;
    
    CS_SuperBlob *attached_super_blob = (CS_SuperBlob *)(slice->file_data + slice->offset + signature->offset);
    if (ntohl(attached_super_blob->magic) == CSMAGIC_EMBEDDED_SIGNATURE) {
        uint32_t count = ntohl(attached_super_blob->count);
        
        for (uint32_t i = 0; i < count; i++) {
            CS_BlobIndex *index = &attached_super_blob->index[i];
            uint32_t type = ntohl(index->type);
            uint32_t offset = ntohl(index->offset);

            CS_GenericBlob *sub_blob = (CS_GenericBlob *)((uint64_t)attached_super_blob + offset);
            uint32_t sub_length = ntohl(attached_super_blob->length) - offset;
            if (sub_length < sizeof(CS_GenericBlob) || sub_length < ntohl(sub_blob->length)) break;
            sub_length = ntohl(sub_blob->length);

            if (type == CSSLOT_ENTITLEMENTS) {
                xml_data = (uint8_t *)sub_blob + sizeof(CS_GenericBlob);
                xml_size = sub_length - sizeof(CS_GenericBlob);
                break;
            }
        }
    }
    
    if (xml_data == NULL || xml_size < 8) return -1;
    CFDataRef cf_data = CFDataCreateWithBytesNoCopy(NULL, xml_data, xml_size, kCFAllocatorNull);
    if (cf_data == NULL) return -1;

    CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, cf_data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(cf_data);
    if (plist == NULL) return -1;
    
    CFDataRef data = CFPropertyListCreateData(NULL, plist, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    CFRelease(plist);
    if (data == NULL) return -1;

    *out_data = calloc(1, CFDataGetLength(data) + 2);
    if (*out_data == NULL) {
        CFRelease(data);
        return -1;
    }

    memcpy(*out_data, CFDataGetBytePtr(data), CFDataGetLength(data));
    *out_size = (uint32_t)CFDataGetLength(data);
    CFRelease(data);
    return 0;
#else
    return -1;
#endif
}

macho_signature_t *macho_get_signature(macho_slice_t *slice) {
    if (slice == NULL) return NULL;
    struct load_command *load_cmd = slice->load_cmd;
    struct linkedit_data_command *linkedit = NULL;

    for (uint32_t i = 0; i < slice->cmd_count; i++){
        if (load_cmd->cmd == LC_CODE_SIGNATURE) {
            linkedit = (struct linkedit_data_command *)load_cmd;
            break;
        }
        load_cmd = (struct load_command *)((uint8_t *)load_cmd + load_cmd->cmdsize);
    }

    if (linkedit == NULL) return NULL;
    CS_SuperBlob *blob = (CS_SuperBlob *)((uint8_t *)(slice->hdr) + linkedit->dataoff);
    CS_CodeDirectory *best_cd = NULL;
    uint32_t best_rank = 0;
    bool is_adhoc = true;
    
    for (uint32_t i = 0; i < ntohl(blob->count); i++) {
        const CS_BlobIndex *index = &blob->index[i];
        uint32_t type = ntohl(index->type);
        uint32_t offset = ntohl(index->offset);
        if (ntohl(blob->length) < offset) break;
        
        if (type == CSSLOT_CODEDIRECTORY || (type >= CSSLOT_ALTERNATE_CODEDIRECTORIES && type < CSSLOT_ALTERNATE_CODEDIRECTORY_LIMIT)) {
            CS_CodeDirectory *cd = (CS_CodeDirectory *)((uint8_t *)blob + offset);
            uint32_t rank = 0;
            switch (cd->hashType) {
                case CS_HASHTYPE_SHA256_TRUNCATED: rank = 2; break;
                case CS_HASHTYPE_SHA256: rank = 3; break;
                case CS_HASHTYPE_SHA384: rank = 4; break;
                case CS_HASHTYPE_SHA1: rank = 1; break;
                default: break;
            }
        
            if (rank > best_rank) {
                best_cd = cd;
                best_rank = rank;
            }
        } else if (type == CSSLOT_SIGNATURESLOT) {
            CS_GenericBlob *signature = (CS_GenericBlob *)((uint8_t *)blob + offset);
            if (ntohl(signature->length) > sizeof(CS_GenericBlob)) is_adhoc = false;
        }
    }

    if (best_rank == 0 || best_cd == NULL) return NULL;
    macho_signature_t *signature = calloc(1, sizeof(macho_signature_t));
    if (signature == NULL) return NULL;

    uint8_t hash_data[64] = {0};
    switch (best_cd->hashType) {
        case CS_HASHTYPE_SHA1: CC_SHA1(best_cd, ntohl(best_cd->length), hash_data); break;
        case CS_HASHTYPE_SHA256:
        case CS_HASHTYPE_SHA256_TRUNCATED: CC_SHA256(best_cd, ntohl(best_cd->length), hash_data); break;
        case CS_HASHTYPE_SHA384: CC_SHA384(best_cd, ntohl(best_cd->length), hash_data); break;
        default: break;
    }
    
    memcpy(signature->hash, hash_data, 20);
    signature->hash_type = best_cd->hashType;
    signature->offset = linkedit->dataoff;
    signature->size = linkedit->datasize;
    signature->code_dir = best_cd;
    signature->version = ntohl(best_cd->version);

    if (is_adhoc && (ntohl(best_cd->flags) & 0x0002) == 0x0002) {
        signature->is_adhoc = true;
    }
    return signature;
}

void macho_release_signature(macho_signature_t *signature) {
    if (signature == NULL) return;
    free(signature);
}

macho_slice_t *macho_get_best_slice(macho_ctx_t *ctx) {
    if (ctx == NULL || ctx->slice_list == NULL || ctx->slice_count == 0) return NULL;
    if (ctx->slice_count == 1) return &ctx->slice_list[0];
    
    uint32_t arm64_idx = -1;
    for (uint32_t i = 0; i < ctx->slice_count; i++) {
        if (ctx->slice_list[i].cpu_type == CPU_TYPE_ARM64) {
            if (ctx->slice_list[i].cpu_subtype == CPU_SUBTYPE_ARM64_V8 || ctx->slice_list[i].cpu_subtype == CPU_SUBTYPE_ARM64_ALL) {
                if (!basebin_info.arm64e_host) return &ctx->slice_list[i];
                arm64_idx = i;
            } else if (basebin_info.arm64e_host && ctx->slice_list[i].has_ptrauth) {
                return &ctx->slice_list[i];
            }
        }
    }
    
    if (arm64_idx != -1) return &ctx->slice_list[arm64_idx];
    return &ctx->slice_list[0];
}

static int super_blob_add(macho_super_blob_t *super_blob, uint32_t type, uint8_t *data, uint32_t size) {
    if (super_blob == NULL || data == NULL || size == 0 || (super_blob->entry_count+1) >= 8) return -1;
    super_blob->entries[super_blob->entry_count].type = type;
    super_blob->entries[super_blob->entry_count].size = size;
    super_blob->entries[super_blob->entry_count].data = data;
    super_blob->entries[super_blob->entry_count++].offset = 0;
    return 0;
}

static int super_blob_build(macho_super_blob_t *super_blob, uint8_t **out_data, uint32_t *out_size) {
    if (super_blob == NULL || out_data == NULL || out_size == 0 || super_blob->entry_count == 0) return -1;
    uint32_t full_size = sizeof(CS_SuperBlob) + (sizeof(CS_BlobIndex) * super_blob->entry_count);
    
    for (uint32_t i = 0; i < super_blob->entry_count; i++) {
        super_blob->entries[i].offset = full_size;
        full_size += super_blob->entries[i].size;
    }

    uint8_t *data = calloc(1, full_size);
    if (data == NULL) return -1;

    CS_SuperBlob *blob = (CS_SuperBlob *)data;
    blob->magic = htonl(CSMAGIC_EMBEDDED_SIGNATURE);
    blob->length = htonl(full_size);
    blob->count = htonl(super_blob->entry_count);

    CS_BlobIndex *index = (CS_BlobIndex *)(data + sizeof(CS_SuperBlob));
    for (uint32_t i = 0; i < super_blob->entry_count; i++) {
        index[i].offset = htonl(super_blob->entries[i].offset);
        index[i].type = htonl(super_blob->entries[i].type);
        memcpy(data + super_blob->entries[i].offset, super_blob->entries[i].data, super_blob->entries[i].size);
    }

    *out_data = data;
    *out_size = full_size;
    return 0;
}

int macho_sign_binary(macho_slice_t *slice, char *ident, uint8_t **out_data, uint32_t *out_size, uint8_t *cd_hash) {
    macho_super_blob_t *super_blob = NULL;
    macho_signature_t *signature = NULL;
    CS_GenericBlob *ents_blob = NULL;
    uint8_t *ents_data = NULL;
    uint32_t ents_size = 0;
    uint8_t *cd_data = NULL;
    int status = -1;

    if ((super_blob = calloc(1, sizeof(macho_super_blob_t))) == NULL) goto done;
    bool is_exec = (slice->file_type == MH_EXECUTE);
    uint32_t code_limit = slice->size;
    uint32_t special_slots = 0;

    signature = macho_get_signature(slice);
    if (signature != NULL) {
        code_limit = ntohl(signature->code_dir->codeLimit);
        if (is_exec && macho_get_entitlements(slice, signature, &ents_data, &ents_size) == 0) {
            special_slots = 5;
        }
    }

    uint32_t target_page_size = basebin_info.page_size;
    uint32_t target_page_mask = target_page_size - 1;
    uint32_t target_page_shift = (target_page_size == 0x1000) ? 0xc : 0xe;
    uint32_t hash_slots = ((code_limit + target_page_mask) & ~target_page_mask) / target_page_size;

    uint32_t cd_size = sizeof(CS_CodeDirectory) + 32 + (hash_slots * 32) + (special_slots * 32);
    if ((cd_data = calloc(1, cd_size)) == NULL) goto done;
    CS_CodeDirectory *code_dir = (CS_CodeDirectory *)cd_data;

    code_dir->magic = htonl(CSMAGIC_CODEDIRECTORY);
    code_dir->length = htonl(cd_size);
    code_dir->version = htonl(0x20400);
    code_dir->identOffset = htonl(sizeof(CS_CodeDirectory));
    code_dir->hashOffset = htonl(sizeof(CS_CodeDirectory) + 32 + (special_slots * 32));
    code_dir->codeLimit = htonl(code_limit);
    code_dir->nSpecialSlots = htonl(special_slots);
    code_dir->nCodeSlots = htonl(hash_slots);
    code_dir->hashSize = 32;
    code_dir->hashType = CS_HASHTYPE_SHA256;
    code_dir->pageSize = target_page_shift;

    uint32_t ident_size = (uint32_t)strnlen(ident, 31);
    uint8_t *special_slot_base = cd_data + sizeof(CS_CodeDirectory) + 32;
    memcpy(cd_data + sizeof(CS_CodeDirectory), ident, ident_size);

    for (uint32_t i = 0; i < hash_slots; i++) {
        uint8_t *hash_slot = cd_data + ntohl(code_dir->hashOffset) + (i * 32);
        uint64_t page_offset = (uint64_t)(i * target_page_size);

        uint8_t *code_data = slice->file_data + slice->offset + page_offset;
        uint32_t code_size = target_page_size;
        if (i == (hash_slots - 1)) code_size = code_limit - (uint32_t)page_offset;
        CC_SHA256(code_data, code_size, hash_slot);
    }

    super_blob_add(super_blob, CSSLOT_CODEDIRECTORY, cd_data, cd_size);
    if (ents_data != NULL) {
        if ((ents_blob = calloc(1, ents_size + sizeof(CS_GenericBlob))) == NULL) goto done;
        ents_blob->magic = htonl(CSMAGIC_EMBEDDED_ENTITLEMENTS);
        ents_blob->length = htonl(ents_size + sizeof(CS_GenericBlob));
        memcpy(ents_blob->data, ents_data, ents_size);

        super_blob_add(super_blob, CSSLOT_ENTITLEMENTS, (uint8_t *)ents_blob, ents_size + sizeof(CS_GenericBlob));
        uint8_t *special_slot = special_slot_base + ((special_slots - CSSLOT_ENTITLEMENTS) * 32);
        CC_SHA256((uint8_t *)ents_blob, ents_size + sizeof(CS_GenericBlob), special_slot);
    }

    CC_SHA256(code_dir, cd_size, cd_hash);
    status = super_blob_build(super_blob, out_data, out_size);

done:
    if (super_blob != NULL) free(super_blob);
    if (ents_blob != NULL) free(ents_blob);
    if (ents_data != NULL) free(ents_data);
    if (signature != NULL) macho_release_signature(signature);
    if (cd_data != NULL) free(cd_data);
    return status;
}

bool macho_should_sign(macho_ctx_t *ctx, macho_signature_t *signature) {
    if (ctx == NULL || signature == false) return false;
    if (signature->version <= 0x20001 || signature->hash_type == CS_HASHTYPE_SHA1) return true;

    if (ctx->path != NULL && strstr(ctx->path, "/var/containers") != NULL) {
        return !signature->is_adhoc;
    }
    return false;
}
