#include "utils.h"
#include "jailbreak.h"
#include "handoff.h"
#include "patchfinder.h"
#include "utils.h"
#include "macho.h"
#include "dma.h"
#include "ppl.h"
#include "trustcache.h"

static jbserver_err_t jbserver_send_msg_with_reply(xpc_object_t request, xpc_object_t *output) {
    struct xpc_global_data *global = NULL;
    if (_os_alloc_once_table[1].once == -1) {
        global = _os_alloc_once_table[1].ptr;
    } else {
        uint32_t num = (kinfo->version[0] == 13) ? 448 : 472;
        global = _os_alloc_once(&_os_alloc_once_table[1], num, NULL);
        if (global != NULL) _os_alloc_once_table[1].once = -1;
    }

    if (global == NULL) return JBSERVER_ERR_CLIENT_FAILURE;
    if (global->xpc_bootstrap_pipe == NULL) {
        mach_port_t *ports = NULL;
        mach_msg_type_number_t count = 0;
        if (mach_ports_lookup(mach_task_self(), &ports, &count) == 0) {
            global->task_bootstrap_port = ports[0];
            global->xpc_bootstrap_pipe = xpc_pipe_create_from_port(global->task_bootstrap_port, 0);
        }
    }

    if (global->xpc_bootstrap_pipe == NULL) return JBSERVER_ERR_CLIENT_FAILURE;
    if (!MACH_PORT_VALID(global->task_bootstrap_port)) {
        global->task_bootstrap_port = MACH_PORT_NULL;
        return JBSERVER_ERR_CLIENT_FAILURE;
    }

    xpc_object_t xpc_pipe = global->xpc_bootstrap_pipe;
    xpc_object_t reply = NULL;
    xpc_pipe_routine(xpc_pipe, request, &reply);
    
    if (reply == NULL) return JBSERVER_ERR_UNKNOWN_FAILURE;
    jbserver_err_t err = (jbserver_err_t)xpc_dictionary_get_int64(reply, "error");
    if (output != NULL) *output = reply;
    else xpc_release(reply);
    return err;
}

static jbserver_err_t jbserver_send_msg(xpc_object_t request) {
    return jbserver_send_msg_with_reply(request, NULL);
}

static xpc_object_t jbserver_init_msg(jbserver_cmd_t cmd) {
    xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(request, "cmd", cmd);
    xpc_dictionary_set_string(request, "name", "com.staturnz.jbserver");
    return request;
}

jbserver_err_t jbserver_heartbeat(void) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_HEARTBEAT);
    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

jbserver_err_t jbserver_platformize(pid_t pid) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_PLATFORMIZE);
    if (pid != getpid()) xpc_dictionary_set_int64(request, "pid", (int64_t)pid);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

jbserver_err_t jbserver_unsandbox(pid_t pid) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_UNSANDBOX);
    if (pid != getpid()) xpc_dictionary_set_int64(request, "pid", (int64_t)pid);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

jbserver_err_t jbserver_set_uid_gid(pid_t pid, uid_t uid, gid_t gid) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_SET_UID_GID);
    if (pid != getpid()) xpc_dictionary_set_int64(request, "pid", (int64_t)pid);
    if (uid != -1) xpc_dictionary_set_int64(request, "uid", (int64_t)uid);
    if (gid != -1) xpc_dictionary_set_int64(request, "gid", (int64_t)gid);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

void jbserver_post_jailbreak(bool root, bool unsandbox) {
    jbserver_platformize(getpid());
    if (unsandbox) {
        jbserver_unsandbox(getpid());
    }

    if (root) {
        jbserver_set_uid_gid(getpid(), 0, 0);
        setuid(0);
        setuid(0);
        setgid(0);
        setgid(0);
    }
}

static int extract_basebins(void) {
    const char *path = NULL;
    FILE *file = NULL;
    int status = -1;
    
    remove_at_path("/amethyst/jbutil");
    remove_at_path("/amethyst/launchd_hook.dylib");
    remove_at_path("/usr/lib/base_hook.dylib");
    remove_at_path("/usr/lib/libjailbreak.dylib");
    
    if ((path = bundle_path("basebins.tar")) == NULL) goto done;
    if ((file = fopen(path, "rb")) == NULL) goto done;

    if (extract_tar(file, "/") != 0) goto done;
    status = 0;
    
done:
    if (path != NULL) free((void *)path);
    if (file != NULL) fclose(file);
    return status;
}

static void add_plist_entry(CFMutableDictionaryRef plist, CFStringRef key, uint64_t value) {
    CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("0x%llx"), value);
    CFDictionaryAddValue(plist, key, value_str);
}

static void init_handoff_plist(void) {
    CFMutableDictionaryRef plist = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    remove_at_path("/amethyst/handoff.plist");

    add_plist_entry(plist, CFSTR("kern_proc_addr"), kinfo->kern_proc_addr);
    add_plist_entry(plist, CFSTR("kern_task_addr"), kinfo->kern_task_addr);
    add_plist_entry(plist, CFSTR("kernel_base"), kinfo->kernel_base);
    add_plist_entry(plist, CFSTR("kernel_slide"), kinfo->kernel_slide);
    add_plist_entry(plist, CFSTR("cpu_ttep"), kinfo->cpu_ttep);
    add_plist_entry(plist, CFSTR("static_trustcache"), kinfo->patches.static_trustcache);
    add_plist_entry(plist, CFSTR("dynamic_trustcache"), kinfo->patches.dynamic_trustcache);
    add_plist_entry(plist, CFSTR("ptov_table"), kinfo->patches.ptov_table);
    add_plist_entry(plist, CFSTR("pidhash_ptr"), kinfo->patches.pidhash_ptr);
    add_plist_entry(plist, CFSTR("pidhashtbl_ptr"), kinfo->patches.pidhashtbl_ptr);
    add_plist_entry(plist, CFSTR("gPhysBase"), kinfo->patches.gPhysBase);
    add_plist_entry(plist, CFSTR("gVirtBase"), kinfo->patches.gVirtBase);
    add_plist_entry(plist, CFSTR("pplrw_entry"), kinfo->patches.pplrw_entry);
    add_plist_entry(plist, CFSTR("pplrw_mapping_va"), kinfo->patches.pplrw_mapping_va);
    add_plist_entry(plist, CFSTR("pplrw_mapping_pa"), kinfo->patches.pplrw_mapping_pa);

    write_plist("/amethyst/handoff.plist", plist);
    CFRelease(plist);
    sync();
}

static int inject_launchd_hook(void) {
    return run_jbutil("--kickstart", NULL, true);
}

int userspace_reboot(void) {
    return run_jbutil("--userspace-reboot", NULL, true);
}

static int preload_basebin(const char *path) {
    if (trustcache_add_binary(path) != 0) return -1;
    chmod(path, 0755);
    sync();
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    usleep(10000);

    if (kinfo->protections.ppl) {
        uint64_t vnode = vnode_for_fd(fd);
        if (vnode == 0) {
            close(fd);
            return -1;
        }

        uint64_t ubc_info = kread64(vnode + koffsetof(vnode, ubcinfo));
        if (ubc_info == 0) {
            close(fd);
            return -1;
        }
        
        if (strstr(path, ".dylib") == NULL) {
            char *args[] = {(char *)path, NULL};
            pid_t pid = -1;
         
            posix_spawnattr_t attr = NULL;
            posix_spawnattr_init(&attr);
            posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
            posix_spawn(&pid, args[0], NULL, &attr, args, NULL);

            if (pid > 0) kill(pid, SIGKILL);
            usleep(10000);
        } else {
            macho_ctx_t *macho = macho_load(path);
            if (macho != NULL) {
                for (uint32_t i = 0; i < macho->slice_count; i++) {
                    macho_signature_t *signature = macho_get_signature(&macho->slice_list[i]);
                    if (signature == NULL) continue;

                    fsignatures_t fsignature = {0};
                    fsignature.fs_file_start = macho->slice_list[i].offset;
                    fsignature.fs_blob_start = (void *)((uint64_t)signature->offset);
                    fsignature.fs_blob_size = signature->size;
                    
                    fcntl(fd, F_ADDFILESIGS, &fsignature);
                    macho_release_signature(signature);
                }
                macho_release(macho);
            }
        }

        
        uint64_t cs_blob = kread64(ubc_info + koffsetof(ubc_info, cs_blob));
        if ((cs_blob & 0xfff0000000000000) != 0xfff0000000000000) {
            close(fd);
            return -1;
        }
        
        while ((cs_blob & 0xfff0000000000000) == 0xfff0000000000000) {
            uint64_t pmap_cs_entry = kread64(cs_blob + 0xb0);
            if (pmap_cs_entry != 0 && pmap_cs_entry != 0xdeadbeefdeadbeef) {
                uint32_t current_trustlevel = kread32(pmap_cs_entry + 0x54);
                if (current_trustlevel != 1) {
                    uint64_t trustlevel_pa = kvtophys(pmap_cs_entry + 0x54);
                    if (trustlevel_pa != 0) ppl_write32(trustlevel_pa, 0x00000003);
                }
            }
            cs_blob = kread64(cs_blob); // cs_blob->csb_next
        }
    } else {
        if (strstr(path, ".dylib") == NULL) {
            char *args[] = {(char *)path, NULL};
            pid_t pid = -1;
            posix_spawnattr_t attr = NULL;
            posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
            posix_spawn(&pid, args[0], NULL, &attr, args, NULL);
            
            if (pid > 0) kill(pid, SIGKILL);
            usleep(10000);
        }
    }
    
    close(fd);
    return 0;
}


int jailbreak_handoff(void) {
    if (extract_basebins() != 0) return -1;
    init_handoff_plist();
    
    preload_basebin("/amethyst/jbutil");
    preload_basebin("/amethyst/launchd_hook.dylib");
    preload_basebin("/usr/lib/base_hook.dylib");
    preload_basebin("/usr/lib/libjailbreak.dylib");
    remove_at_path("/amethyst/update");

    if (jbserver_heartbeat() == 0x1337) return 0;
    add_cs_flag(1, CS_VALID|CS_SIGNED|CS_GET_TASK_ALLOW|CS_DEBUGGED|CS_PLATFORM_BINARY|CS_INVALID_ALLOWED);
    remove_cs_flag(1, CS_HARD|CS_RESTRICT|CS_KILL|CS_REQUIRE_LV|CS_ENFORCEMENT|CS_FORCED_LV);
    set_mac_slot(1, 1, 0);

    if (kinfo->protections.ppl) {
        uint64_t task = find_task_for_pid(1);
        uint64_t vm_map = kread64(task + koffsetof(task, vm_map));
        uint64_t task_pmap = kread64(vm_map + koffsetof(vm_map, pmap));
        ppl_write8(kvtophys(task_pmap + koffsetof(pmap, cs_enforced)), 0);
    }
    
    chown("/usr", 0, 0);
    chown("/usr/lib", 0, 0);
    chown("/usr/libexec", 0, 0);
    chmod("/usr", 0755);
    chmod("/usr/lib", 0755);
    chmod("/usr/libexec", 0755);
    sync();

    for (int i = 0; i < 1; i++) {
        bool success = false;
        inject_launchd_hook();
        usleep(100000);
        
        for (int j = 0; j < 10; j++) {
            if (jbserver_heartbeat() == 0x1337) {
                success = true;
                break;
            }
            usleep(25000);
        }
        if (success) break;
    }
    return (jbserver_heartbeat() == 0x1337) ? 0 : -1;
}
