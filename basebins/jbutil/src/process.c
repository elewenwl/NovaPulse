#include "dyld.h"
#include "process.h"

uint64_t process_alloc(process_ctx_t *ctx, uint32_t size) {
    mach_vm_address_t addr = 0;
    if (mach_vm_allocate(ctx->task, &addr, size, VM_FLAGS_ANYWHERE) != 0) return 0;
    return (uint64_t)addr;
}

int process_dealloc(process_ctx_t *ctx, uint64_t addr, size_t size) {
    return mach_vm_deallocate(ctx->task, addr, size);
}

int process_protect(process_ctx_t *ctx, uint64_t addr, uint32_t size, vm_prot_t prot) {
    mach_vm_protect(ctx->task, addr, size, 1, prot);
    return mach_vm_protect(ctx->task, addr, size, 0, prot);
}

int process_read(process_ctx_t *ctx, uint64_t addr, void *buf, uint32_t size) {
    mach_vm_size_t read_size = 0;
    return mach_vm_read_overwrite(ctx->task, (mach_vm_address_t)addr, (mach_vm_size_t)size, (mach_vm_address_t)buf, &read_size);
}

int process_write(process_ctx_t *ctx, uint64_t addr, void *buf, uint32_t size) {
    return mach_vm_write(ctx->task, (mach_vm_address_t)addr, (vm_offset_t)buf, (mach_msg_type_number_t)size);
}

uint64_t process_read64(process_ctx_t *ctx, uint64_t addr) {
    uint64_t value = 0;
    process_read(ctx, addr, &value, 8);
    return value;
}

uint32_t process_read32(process_ctx_t *ctx, uint64_t addr) {
    uint32_t value = 0;
    process_read(ctx, addr, &value, 4);
    return value;
}

void process_write64(process_ctx_t *ctx, uint64_t addr, uint64_t value) {
    process_write(ctx, addr, &value, 8);
}

void process_write32(process_ctx_t *ctx, uint64_t addr, uint32_t value) {
    process_write(ctx, addr, &value, 4);
}

uintptr_t process_create_str(process_ctx_t *ctx, char *str) {
    uint32_t size = (uint32_t)strlen(str) + 2;
    uintptr_t addr = process_alloc(ctx, size);
    if (addr == 0) return 0;

    process_write(ctx, addr, str, size-2);
    return addr;
}

pid_t process_get_pid(const char *name) {
    if (name == NULL) return -1;
    char current_path[PROC_PIDPATHINFO_MAXSIZE+1] = {0};
    pid_t target_pid = -1;

    int count = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (count <= 0) return -1;

    uint32_t list_size = sizeof(pid_t) * (count+20);
    pid_t *pid_list = calloc(1, list_size);
    if (pid_list == NULL) return -1;

    count = proc_listpids(PROC_ALL_PIDS, 0, pid_list, list_size);
    for (int i = 0; i < count; i++) {
        int current_pid = pid_list[i];
        if (current_pid <= 1) continue;
        bzero(current_path, PROC_PIDPATHINFO_MAXSIZE);

        proc_pidpath(current_pid, current_path, PROC_PIDPATHINFO_MAXSIZE);
        char *current_name = basename(current_path);

        if (current_name != NULL && strncmp(name, current_name, MAXCOMLEN) == 0) {
            target_pid = current_pid;
            break;
        }
    }

    free(pid_list);
    return target_pid;
}

int process_send_signal(pid_t pid, int num) {
    if (pid <= 1 || getpid() == pid) return -1;
    return kill(pid, num);
}

int process_get_root(void) {
    if (getuid() == 0) return 0;
    if (access("/usr/lib/libjailbreak.dylib", F_OK) == 0) {
        void *handle = dlopen("/usr/lib/libjailbreak.dylib", RTLD_LAZY);
        if (handle == NULL) return -1;

        void (*libjb_uid)(pid_t pid) = dlsym(handle, "jb_oneshot_fix_setuid_now");
        void (*libjb_platform)(pid_t pid, uint32_t flags) = dlsym(handle, "jb_oneshot_entitle_now");

        if (libjb_uid != NULL) libjb_uid(getpid());
        if (libjb_platform != NULL) libjb_platform(getpid(), FLAG_PLATFORMIZE);        
    }

    setuid(0);
    setgid(0);
    usleep(10000);
    return (getuid() == 0) ? 0 : -1;
}

int process_run(const char *path, char **argv, bool use_root) {
    if (path == NULL || argv == NULL) return -1;
    if (use_root && process_get_root() != 0) return -1;
    return execvp(path, argv);
}

uint64_t process_find_image(process_ctx_t *ctx, const char *path) {
    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;

    task_info(ctx->task, TASK_DYLD_INFO, (task_info_t)&info, &count);
    if (info.all_image_info_addr == 0) return 0;

    struct dyld_all_image_infos image_info = {0};
    if (process_read(ctx, info.all_image_info_addr, &image_info, sizeof(struct dyld_all_image_infos)) != 0) return 0;

    struct dyld_image_info current_image = {0};
    char current_path[PATH_MAX] = {0};

    for (uint32_t i = 0; i < image_info.infoArrayCount; i++) {
        if (process_read(ctx, (uint64_t)image_info.infoArray + (i * sizeof(struct dyld_image_info)), &current_image, sizeof(struct dyld_image_info)) != 0) continue;
        if (process_read(ctx, (uint64_t)current_image.imageFilePath, &current_path[0], sizeof(current_path)-1) != 0) continue;
        if (strstr(path, current_path) != NULL) return (uint64_t)current_image.imageLoadAddress;
    }
    return 0;
}

uint64_t process_find_private_symbol(process_ctx_t *ctx, const char *path, const char *name) {
    dyld_cache_header_t hdr = {};
#ifdef __arm64e__
    FILE *dsc_file = fopen(DSC_ARM64E, "rb");
#else
    FILE *dsc_file = fopen(DSC_ARM64, "rb");
#endif
    
    if (dsc_file == NULL) return 0;
    fread(&hdr, sizeof(dyld_cache_header_t), 1, dsc_file);
    
    dyld_cache_image_info_t image_info = {0};
    uint32_t image_offset = hdr.imagesOffset;
    uint32_t image_idx = -1;

    for (uint32_t i = 0; i < hdr.imagesCount; i++) {
        fseek(dsc_file, image_offset, SEEK_SET);
        fread(&image_info, sizeof(dyld_cache_image_info_t), 1, dsc_file);
        image_offset += sizeof(dyld_cache_image_info_t);

        char path_buf[PATH_MAX] = {0};
        fseek(dsc_file, image_info.pathFileOffset, SEEK_SET);
        fread(path_buf, PATH_MAX, 1, dsc_file);
        if (strcmp(path, path_buf) == 0) {
            image_idx = i;
            break;
        }
    }
    
    if (image_idx == -1 || image_idx > hdr.imagesCount) {
        fclose(dsc_file);
        return 0;
    }
    
    dyld_cache_local_symbols_info_t symbol_info = {0};
    fseek(dsc_file, hdr.localSymbolsOffset, SEEK_SET);
    fread(&symbol_info, sizeof(dyld_cache_local_symbols_info_t), 1, dsc_file);

    dyld_cache_local_symbols_entry_t entry = {0};
    uint64_t entry_offset = hdr.localSymbolsOffset + symbol_info.entriesOffset;
    if (image_idx > 0) entry_offset += sizeof(dyld_cache_local_symbols_entry_t) * image_idx;
    fseek(dsc_file, entry_offset, SEEK_SET);
    fread(&entry, sizeof(dyld_cache_local_symbols_entry_t), 1, dsc_file);

    uint64_t symbol_offset = hdr.localSymbolsOffset + symbol_info.nlistOffset;
    uint64_t string_offset = hdr.localSymbolsOffset + symbol_info.stringsOffset;
    uint64_t symbol_addr = 0;

    for (uint64_t i = entry.nlistStartIndex; i < entry.nlistStartIndex + entry.nlistCount; i++) {
        struct nlist_64 nlist = {};
        fseek(dsc_file, symbol_offset + (sizeof(struct nlist_64) * i), SEEK_SET);
        fread(&nlist, sizeof(struct nlist_64), 1, dsc_file);

        fseek(dsc_file, string_offset + nlist.n_un.n_strx, SEEK_SET);
        char symbol_name[0x100] = {0};
        fread(symbol_name, strlen(name)+1, 1, dsc_file);

        if (strncmp(symbol_name, name, strlen(name)) == 0) {
            fseek(dsc_file, string_offset + nlist.n_un.n_strx, SEEK_SET);
            fread(symbol_name, 0xff, 1, dsc_file);
            if (strcmp(symbol_name, name) == 0) {
                symbol_addr = nlist.n_value;
                break;
            }
        }
    }
    
    fclose(dsc_file);
    if (symbol_addr == 0) return 0;

    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;

    task_info(ctx->task, TASK_DYLD_INFO, (task_info_t)&info, &count);
    if (info.all_image_info_addr == 0) return 0;

    struct dyld_all_image_infos all_image_infos = {0};
    if (process_read(ctx, info.all_image_info_addr, &all_image_infos, sizeof(struct dyld_all_image_infos)) != 0) return 0;
    return all_image_infos.sharedCacheSlide + symbol_addr;
}

uint64_t process_find_symbol(process_ctx_t *ctx, uintptr_t image, const char *name) {
    size_t name_len = strlen(name);
    uint64_t stroff = 0;
    uint64_t symoff = 0;
    uint64_t slide = 0;

    struct mach_header_64 hdr = {0};
    struct load_command load_cmd = {0};
    struct symtab_command sym_cmd = {0};
    uint64_t cmd_addr = image + sizeof(struct mach_header_64);

    if (process_read(ctx, image, &hdr, sizeof(struct mach_header_64)) != 0) return 0;
    if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;

    for (uint32_t i = 0; i < hdr.ncmds; i++) {
        if (load_cmd.cmd == LC_SYMTAB) {
            if (process_read(ctx, cmd_addr, &sym_cmd, sizeof(struct symtab_command)) != 0) return 0;
            break;
        }
        
        cmd_addr += load_cmd.cmdsize;
        if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;
    }

    if (sym_cmd.stroff == 0) return 0;
    cmd_addr = image + sizeof(struct mach_header_64);
    if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;

    for (uint32_t i = 0; i < hdr.ncmds; i++) {
        if (load_cmd.cmd == LC_SEGMENT_64) {
            struct segment_command_64 segment = {0};
            if (process_read(ctx, cmd_addr, &segment, sizeof(struct segment_command_64)) != 0) return 0;

            if (slide == 0 && strncmp(segment.segname, "__TEXT", 16) == 0) {
                slide = segment.vmaddr;
            } else if (strncmp(segment.segname, "__LINKEDIT", 16) == 0) {
                if ((sym_cmd.symoff - segment.fileoff) < segment.filesize) {
                    symoff = segment.vmaddr + sym_cmd.symoff - segment.fileoff;
                }
                
                if ((sym_cmd.stroff - segment.fileoff) < segment.filesize) {
                    stroff = segment.vmaddr + sym_cmd.stroff - segment.fileoff;
                }
            }
            if (stroff != 0 && symoff != 0 && slide != 0) break;
        }

        cmd_addr += load_cmd.cmdsize;
        if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;
    }

    if (slide != 0) {
        stroff -= slide;
        symoff -= slide;
    }

    for (int i = 0; i < sym_cmd.nsyms; i++) {
        struct nlist_64 symbol = {};
        if (process_read(ctx, image + symoff + (i * sizeof(struct nlist_64)), &symbol, sizeof(struct nlist_64)) != 0) return 0;
        uint64_t str_idx = symbol.n_un.n_strx;

        if (str_idx >= sym_cmd.strsize || str_idx == 0) continue;
        if ((symbol.n_type & N_PEXT) == N_PEXT && (symbol.n_type & N_EXT) == N_EXT) continue;
        if ((symbol.n_type & N_STAB) != 0 && (symbol.n_type & N_TYPE) == N_ABS) continue;

        char current_name[PATH_MAX] = {0};
        if (process_read(ctx, image + stroff + str_idx, &current_name[0], name_len + 1) != 0) return 0;

        if (strcmp(current_name, name) == 0) {
            if (symbol.n_value == 0) continue;
            return (uint64_t)(image + symbol.n_value) - slide;
        }
    }
    return 0;
}

uint64_t process_find_spin_gadget(process_ctx_t *ctx) {
    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;

    task_info(ctx->task, TASK_DYLD_INFO, (task_info_t)&info, &count);
    if (info.all_image_info_addr == 0) return 0;

    struct dyld_image_info current_image = {0};
    struct dyld_all_image_infos image_info = {0};
    if (process_read(ctx, info.all_image_info_addr, &image_info, sizeof(struct dyld_all_image_infos)) != 0) return 0;

    for (uint32_t i = 0; i < image_info.infoArrayCount; i++) {
        if (process_read(ctx, (uint64_t)image_info.infoArray + (i * sizeof(struct dyld_image_info)), &current_image, sizeof(struct dyld_image_info)) != 0) continue;
        uint64_t image = (uint64_t)current_image.imageLoadAddress;
        uint64_t cmd_addr = image + sizeof(struct mach_header_64);
        struct mach_header_64 hdr = {0};
        struct load_command load_cmd = {0};

        if (process_read(ctx, image, &hdr, sizeof(struct mach_header_64)) != 0) return 0;
        if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;

        for (uint32_t j = 0; j < hdr.ncmds; j++) {
            if (load_cmd.cmd == LC_SEGMENT_64) {
                struct segment_command_64 segment = {0};
                if (process_read(ctx, cmd_addr, &segment, sizeof(struct segment_command_64)) != 0) return 0;

                if (strncmp(segment.segname, "__TEXT", 16) == 0) {
                    for (uint32_t k = 0; k < segment.nsects; k++) {
                        struct section_64 section = {};
                        uint64_t read_offset = sizeof(struct segment_command_64) + (sizeof(struct section_64) * k);
                        if (process_read(ctx, cmd_addr + read_offset, &section, sizeof(struct section_64)) != 0) continue;

                        if (strncmp(section.sectname, "__text", 16) == 0) {
                            uint32_t *search_data = calloc(1, section.size);
                            if (search_data == NULL) continue;
                            if (process_read(ctx, image + section.offset, search_data, section.size) != 0) {
                                free(search_data);
                                continue;
                            }

                            for (int l = 0; l < section.size/0x4; l++) {
                                if (search_data[l] == 0x14000000) {
                                    free(search_data);
                                    return image + section.offset + (l*0x4);
                                }
                            }
                        }
                    }
                }
            }
            
            cmd_addr += load_cmd.cmdsize;
            if (process_read(ctx, cmd_addr, &load_cmd, sizeof(struct load_command)) != 0) return 0;
        }
    }
    return 0;
}

void process_release(process_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (MACH_PORT_VALID(ctx->task)) mach_port_deallocate(mach_task_self(), ctx->task);
    bzero(ctx, sizeof(process_ctx_t));
    free(ctx);
}

mach_port_t process_get_task(pid_t pid) {
    mach_port_t task = MACH_PORT_NULL;
    int kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != 0 || !MACH_PORT_VALID(task)) return MACH_PORT_NULL;

    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(task, TASK_DYLD_INFO, (task_info_t)&info, &count) == KERN_INVALID_ARGUMENT) {
        void *handle = dlopen("/usr/lib/libjailbreak.dylib", RTLD_LAZY);
        if (handle != NULL) {
            void (*ptr)(pid_t, uint32_t) = dlsym(handle, "jb_oneshot_entitle_now");
            if (ptr != NULL) ptr(getpid(), (1 << 1));
            dlclose(handle);
            
            usleep(10000);
            if (task_info(task, TASK_DYLD_INFO, (task_info_t)&info, &count) == 0) return task;
        }

        mach_port_deallocate(mach_task_self(), task);
        task = MACH_PORT_NULL;
    }
    return task;
}

process_ctx_t *process_init(pid_t pid) {
    if (pid <= 0) return NULL;
    process_ctx_t *ctx = calloc(1, sizeof(process_ctx_t));
    if (ctx == NULL) return NULL;
    int status = -1;

    ctx->pid = pid;
    ctx->task = process_get_task(pid);
    if (!MACH_PORT_VALID(ctx->task)) goto done;

    task_dyld_info_data_t info = {0};
    uint32_t count = TASK_DYLD_INFO_COUNT;
    task_info(ctx->task, TASK_DYLD_INFO, (task_info_t)&info, &count);

    if (info.all_image_info_addr == 0) goto done;
    struct dyld_all_image_infos image_info = {0};
    if (process_read(ctx, info.all_image_info_addr, &image_info, sizeof(struct dyld_all_image_infos)) != 0) goto done;

    struct dyld_image_info first_image = {0};
    if (process_read(ctx, (uint64_t)image_info.infoArray, &first_image, sizeof(struct dyld_image_info)) != 0) goto done;
    ctx->image_base = (uint64_t)first_image.imageLoadAddress;

    struct mach_header hdr = {0};
    if (process_read(ctx, ctx->image_base, &hdr, sizeof(struct mach_header)) != 0) goto done;
    ctx->arch = PROCESS_ARCH_UNKNOWN;

    if (hdr.cputype == CPU_TYPE_ARM) {
        ctx->is_64bit = false;
        if (hdr.cpusubtype == CPU_SUBTYPE_ARM_ALL || hdr.cpusubtype == CPU_SUBTYPE_ARM_V7 || hdr.cpusubtype == CPU_SUBTYPE_ARM_V7F || hdr.cpusubtype == CPU_SUBTYPE_ARM_V7K) {
            ctx->arch = PROCESS_ARCH_ARMV7;
        } else if (hdr.cpusubtype == CPU_SUBTYPE_ARM_V7S) {
            ctx->arch = PROCESS_ARCH_ARMV7S;
        } else if (hdr.cpusubtype == CPU_SUBTYPE_ARM_V6) {
            ctx->arch = PROCESS_ARCH_ARMV6;
        }
    } else if (hdr.cputype == CPU_TYPE_ARM64) {
        ctx->is_64bit = true;
        if (hdr.cpusubtype == CPU_SUBTYPE_ARM64_ALL || hdr.cpusubtype == CPU_SUBTYPE_ARM64_V8) {
            ctx->arch = PROCESS_ARCH_ARM64;
        } else if (hdr.cpusubtype == CPU_SUBTYPE_ARM64E || hdr.cpusubtype == (CPU_SUBTYPE_PTRAUTH_ABI | CPU_SUBTYPE_ARM64E)) {
            ctx->arch = PROCESS_ARCH_ARM64E;
        }
    }
    
    if (ctx->arch == PROCESS_ARCH_UNKNOWN) goto done;
    status = 0;

done:
    if (status == 0) return ctx;
    process_release(ctx);
    return NULL;
}
