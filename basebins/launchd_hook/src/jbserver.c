#include "util.h"
#include "memory.h"
#include "codesign.h"
#include "loader.h"
#include "memory.h"
#include "basebin_hook.h"
#include "ppl.h"
#include "trustcache.h"
#include "jbserver.h"

static xpc_object_t (*xpc_serializer_unpack_orig)(void *, void *, void *) = NULL;
static int (*xpc_pipe_handle_mig_orig)(void *a1, void *a2, void *a3) = NULL;
static int (*xpc_receive_mach_msg_orig)(void *a1, void *a2, void *a3, void *a4, xpc_object_t *output) = NULL;
static xpc_object_t (*xpc_dictionary_get_value_orig)(xpc_object_t dict, const char *key) = NULL;
static int (*sandbox_check_by_audit_token_orig)(audit_token_t, const char *, int, ...) = NULL;

static lookup_info_t lookup_allow_list[] = {
    {"cy:", 3},
    {"lh:", 3},
    {"com.ex.substituted", 18},
    {"com.apple.ReportCrash.SimulateCrash", 35},
    {NULL, 0}
};

static pid_t jbserver_getpid(xpc_object_t request) {
    pid_t pid = (pid_t)xpc_dictionary_get_int64(request, "pid");
    if (pid > 0) return pid;

    audit_token_t token = {0};
    xpc_dictionary_get_audit_token(request, &token);
    pid = get_audit_token_pid(token);
    return (pid <= 1) ? -1 : pid;
}

static uid_t jbserver_getuid(xpc_object_t request) {
    pid_t pid = (pid_t)xpc_dictionary_get_int64(request, "pid");
    if (pid > 0) return -1; // caller pid != target pid

    audit_token_t token = {0};
    xpc_dictionary_get_audit_token(request, &token);
    return get_audit_token_uid(token);
}


static gid_t jbserver_getgid(xpc_object_t request) {
    pid_t pid = (pid_t)xpc_dictionary_get_int64(request, "pid");
    if (pid > 0) return -1; // caller pid != target pid

    audit_token_t token = {0};
    xpc_dictionary_get_audit_token(request, &token);
    return get_audit_token_gid(token);
}

static jbserver_err_t init_process(pid_t pid, uid_t uid, gid_t gid, bool unsandbox) {
    uint64_t proc = 0;
    uint64_t task = 0;
    uint64_t ucred = 0;

    if (get_process_info(pid, &proc, &task) != 0) return JBSERVER_ERR_INVALID_PROCESS;
    uint32_t add_flags = (CS_PLATFORM_BINARY|CS_VALID|CS_DEBUGGED|CS_INVALID_ALLOWED|CS_GET_TASK_ALLOW|CS_EXECSEG_SKIP_LV);
    uint32_t remove_flags = (CS_RESTRICT|CS_HARD|CS_KILL|CS_REQUIRE_LV|CS_FORCED_LV|CS_ENFORCEMENT);

    uint32_t cs_flags = kread32(proc + koffsetof(proc, csflags));
    cs_flags &= ~remove_flags;
    cs_flags |= add_flags;

    kwrite32(proc + koffsetof(proc, csflags), cs_flags);
    add_task_flag(-1, task, TF_PLATFORM);
    if (unsandbox) set_mac_slot(-1, proc, SANDBOX_SLOT, 0);

    if (uid != -1 || gid != -1) {
        uint32_t proc_flags = kread32(proc + koffsetof(proc, p_flag));
        if ((proc_flags & P_SUGID) == P_SUGID) {
            kwrite32(proc + koffsetof(proc, p_flag), proc_flags & ~P_SUGID);
        }

        if (uid != -1) {
            ucred = kread64(proc + koffsetof(proc, ucred));
            if (KADDR_VALID(ucred)) {
                kwrite32(proc + koffsetof(proc, p_svuid), uid);
                kwrite32(ucred + koffsetof(ucred, cr_svuid), uid);
                kwrite32(ucred + koffsetof(ucred, cr_uid), uid);
            }
        }

        if (gid != -1) {
            if (!KADDR_VALID(ucred)) ucred = kread64(proc + koffsetof(proc, ucred));
            if (KADDR_VALID(ucred)) {
                kwrite32(proc + koffsetof(proc, p_svgid), gid);
                kwrite32(ucred + koffsetof(ucred, cr_svgid), gid);
                kwrite32(ucred + koffsetof(ucred, cr_groups), gid);
            }
        }
    }

#if defined(__arm64e__)
    uint64_t vm_map = kread64(task + koffsetof(task, vm_map));
    if (!KADDR_VALID(vm_map)) return JBSERVER_ERR_INVALID_PROCESS;

    uint64_t pmap = kread64(vm_map + koffsetof(vm_map, pmap));
    if (!KADDR_VALID(pmap)) return JBSERVER_ERR_INVALID_PROCESS;

    if (kread8(pmap + koffsetof(pmap, cs_enforced)) == 1) {
        ppl_write8(pmap + koffsetof(pmap, cs_enforced), 0);

        uint64_t pmap_cs_entry = proc_get_pmap_cs_entry(proc);
        if (pmap_cs_entry != 0) {
            uint32_t current_trustlevel = kread32(pmap_cs_entry + koffsetof(pmap_cs_code_directory, trust));
            if (current_trustlevel != 1) {
               ppl_write32(pmap_cs_entry + koffsetof(pmap_cs_code_directory, trust), 1);
            }
        }
        if (kread8(pmap + koffsetof(pmap, cs_enforced)) == 1) return JBSERVER_ERR_UNKNOWN_FAILURE;
    }
#endif
    return JBSERVER_ERR_SUCCESS;
}

static jbserver_err_t jbserver_trustcache_mach(jbserver_trustcache_msg_t *msg) {
    if (msg->base.hdr.msgh_size != sizeof(jbserver_trustcache_msg_t)) return JBSERVER_ERR_INVALID_REQUEST;
    if (!trustcache_lock_check(msg->hash)) {
        trustcache_lock_add_hash(msg->hash, (msg->type & 0xff));
    }
    return JBSERVER_ERR_SUCCESS;
}

static jbserver_err_t jbserver_init_process_mach(jbserver_init_process_msg_t *msg) {
    if (msg->base.hdr.msgh_size != sizeof(jbserver_init_process_msg_t)) return JBSERVER_ERR_INVALID_REQUEST;
    if (msg->base.pid <= 0) return JBSERVER_ERR_INVALID_PROCESS;
    return init_process(msg->base.pid, -1, -1, false);
}

static jbserver_err_t jbserver_trustcache(xpc_object_t request, xpc_object_t reply) {
    xpc_object_t hash_object = xpc_dictionary_get_value_orig(request, "hash");
    if (hash_object == NULL) return JBSERVER_ERR_INVALID_CD_HASH;
    uint8_t hash[20] = {0};
    uint8_t type = -1;

    __unused size_t size = xpc_data_get_bytes(hash_object, &hash[0], 0, 20);
    xpc_object_t type_object = xpc_dictionary_get_value_orig(request, "type");
    if (type_object != NULL) type = (uint8_t)(xpc_uint64_get_value(type_object) & 0xff);

    if (!trustcache_lock_check(hash)) {
        trustcache_lock_add_hash(hash, type);
    }
    return JBSERVER_ERR_SUCCESS;
}

static jbserver_err_t jbserver_sign_binary(xpc_object_t request, xpc_object_t reply) {
    const char *path = xpc_dictionary_get_string(request, "path");
    if (path == NULL) return JBSERVER_ERR_INVALID_PATH;
    
    uint32_t le_offset = (uint32_t)xpc_dictionary_get_uint64(request, "le_offset");
    uint32_t le_size = (uint32_t)xpc_dictionary_get_uint64(request, "le_size");
    uint32_t slice_offset = (uint32_t)xpc_dictionary_get_uint64(request, "slice_offset");
    uint32_t file_type = (uint32_t)xpc_dictionary_get_uint64(request, "file_type");
    
    int status = sign_binary((char *)path, le_offset, le_size, slice_offset, file_type);
    return (status == 0) ? JBSERVER_ERR_SUCCESS : JBSERVER_ERR_UNKNOWN_FAILURE;
}

static jbserver_err_t jbserver_init_process(xpc_object_t request, xpc_object_t reply) {
    pid_t pid = jbserver_getpid(request);
    if (pid <= 0) return JBSERVER_ERR_INVALID_PROCESS;
    uid_t target_uid = -1;
    gid_t target_gid = -1;

    xpc_object_t xpc_target_uid = xpc_dictionary_get_value_orig(request, "uid");
    if (xpc_target_uid != NULL && xpc_get_type(xpc_target_uid) && XPC_TYPE_INT64) {
        uid_t requested_uid = (uid_t)xpc_int64_get_value(xpc_target_uid);
        if (jbserver_getuid(request) != requested_uid) target_uid = requested_uid;
    }

    xpc_object_t xpc_target_gid = xpc_dictionary_get_value_orig(request, "gid");
    if (xpc_target_gid != NULL && xpc_get_type(xpc_target_uid) && XPC_TYPE_INT64) {
        gid_t requested_gid = (gid_t)xpc_int64_get_value(xpc_target_gid);
        if (jbserver_getgid(request) != requested_gid) target_gid = requested_gid;
    }

    bool unsandbox = xpc_dictionary_get_bool(request, "unsandbox");
    return init_process(pid, target_uid, target_gid, unsandbox);
}

static jbserver_err_t jbserver_platformize(xpc_object_t request, xpc_object_t reply) {
    pid_t pid = jbserver_getpid(request);
    if (pid <= 0) return JBSERVER_ERR_INVALID_PROCESS;

    uint64_t proc = 0;
    uint64_t task = 0;
    if (get_process_info(pid, &proc, &task) != 0) return JBSERVER_ERR_INVALID_PROCESS;

    add_cs_flag(-1, proc, CS_PLATFORM_BINARY);
    add_task_flag(-1, task, TF_PLATFORM);
    return JBSERVER_ERR_SUCCESS;
}

static jbserver_err_t jbserver_unsandbox(xpc_object_t request, xpc_object_t reply) {
    pid_t pid = jbserver_getpid(request);
    if (pid <= 0) return JBSERVER_ERR_INVALID_PROCESS;
    
    set_mac_slot(pid, 0, SANDBOX_SLOT, 0);
    return JBSERVER_ERR_SUCCESS;
}

static jbserver_err_t jbserver_set_uid_gid(xpc_object_t request, xpc_object_t reply) {
    pid_t pid = jbserver_getpid(request);
    if (pid <= 0) return JBSERVER_ERR_INVALID_PROCESS;

    uint64_t proc = find_proc_for_pid(pid);
    if (!KADDR_VALID(proc)) return JBSERVER_ERR_INVALID_PROCESS;

    uint64_t ucred = kread64(proc + koffsetof(proc, ucred));
    if (!KADDR_VALID(ucred)) return JBSERVER_ERR_INVALID_PROCESS;
    bool legacy = xpc_dictionary_get_bool(request, "legacy");
    bool id_changed = false;

    xpc_object_t xpc_target_uid = xpc_dictionary_get_value_orig(request, "uid");
    if (xpc_target_uid != NULL && xpc_get_type(xpc_target_uid) && XPC_TYPE_INT64) {
        uid_t target_uid = (uid_t)xpc_int64_get_value(xpc_target_uid);
        
        if (jbserver_getuid(request) != target_uid) {
            if (!legacy) {
                kwrite32(proc + koffsetof(proc, p_svuid), target_uid);
                kwrite32(ucred + koffsetof(ucred, cr_svuid), target_uid);
            }

            id_changed = true;
            kwrite32(ucred + koffsetof(ucred, cr_uid), target_uid);
        }
    }

    xpc_object_t xpc_target_gid = xpc_dictionary_get_value_orig(request, "gid");
    if (xpc_target_gid != NULL && xpc_get_type(xpc_target_uid) && XPC_TYPE_INT64) {
        gid_t target_gid = (uid_t)xpc_int64_get_value(xpc_target_gid);

        if (jbserver_getgid(request) != target_gid) {
            if (!legacy) {
                kwrite32(proc + koffsetof(proc, p_svgid), target_gid);
                kwrite32(ucred + koffsetof(ucred, cr_svgid), target_gid);
            }

            id_changed = true;
            kwrite32(ucred + koffsetof(ucred, cr_groups), target_gid);
        }
    }

    if (id_changed) {
        uint32_t proc_flags = kread32(proc + koffsetof(proc, p_flag));
        if ((proc_flags & P_SUGID) == P_SUGID) {
            kwrite32(proc + koffsetof(proc, p_flag), proc_flags & ~P_SUGID);
        }
    }
    return JBSERVER_ERR_SUCCESS;
}

static void jbserver_handle_request(xpc_object_t request, jbserver_base_msg_t *msg) {
    if (request != NULL) {
        jbserver_cmd_t cmd = (jbserver_cmd_t)xpc_dictionary_get_uint64(request, "cmd");
        jbserver_err_t err = JBSERVER_ERR_UNKNOWN_FAILURE;
        xpc_object_t reply = xpc_dictionary_create_reply(request);

        switch (cmd) {
            case JBSERVER_CMD_TRUSTCACHE: err = jbserver_trustcache(request, reply); break;
            case JBSERVER_CMD_SIGN_BINARY: err = jbserver_sign_binary(request, reply); break;
            case JBSERVER_CMD_INIT_PROCESS: err = jbserver_init_process(request, reply); break;
            case JBSERVER_CMD_PLATFORMIZE: err = jbserver_platformize(request, reply); break;
            case JBSERVER_CMD_UNSANDBOX: err = jbserver_unsandbox(request, reply); break;
            case JBSERVER_CMD_SET_UID_GID: err = jbserver_set_uid_gid(request, reply); break;
            case JBSERVER_CMD_HEARTBEAT: err = 0x1337; break;
            default: err = JBSERVER_ERR_UNKNOWN_CMD; break;
        }

        xpc_dictionary_set_int64(reply, "error", err);
        xpc_pipe_routine_reply(reply);
        xpc_release(reply);
    } else if (msg != NULL) {
        jbserver_cmd_t cmd = (jbserver_cmd_t)msg->cmd;
        jbserver_err_t err = JBSERVER_ERR_UNKNOWN_FAILURE;

        switch (cmd) {
            case JBSERVER_CMD_TRUSTCACHE: err = jbserver_trustcache_mach((jbserver_trustcache_msg_t *)msg); break;
            case JBSERVER_CMD_INIT_PROCESS: err = jbserver_init_process_mach((jbserver_init_process_msg_t *)msg); break;
            case JBSERVER_CMD_HEARTBEAT: err = 0x1337; break;
            default: err = JBSERVER_ERR_UNKNOWN_CMD; break;
        }

        if (MACH_PORT_VALID(msg->hdr.msgh_remote_port)) {
            mach_msg_bits_t reply_bits = MACH_MSGH_BITS_REMOTE(msg->hdr.msgh_bits);
            if (reply_bits == 0) return;

            if (reply_bits == MACH_MSG_TYPE_COPY_SEND) reply_bits = MACH_MSG_TYPE_MOVE_SEND;
            jbserver_reply_msg_t reply = {};

            reply.hdr.msgh_bits = MACH_MSGH_BITS(reply_bits, 0);
            reply.hdr.msgh_size = sizeof(jbserver_reply_msg_t);
            reply.hdr.msgh_remote_port = msg->hdr.msgh_remote_port;
            reply.hdr.msgh_local_port = MACH_PORT_NULL;
            reply.hdr.msgh_voucher_port = MACH_PORT_NULL;
            reply.hdr.msgh_id = msg->hdr.msgh_id + 100;
            reply.error = err;

            if (mach_msg_send(&reply.hdr) == 0) {
                msg->hdr.msgh_bits &= ~MACH_MSGH_BITS_REMOTE_MASK;
                msg->hdr.msgh_remote_port = MACH_PORT_NULL;
            }
        }
    }
}

static void jbserver_prepare_reboot(void) {
    kinfo->userspace_rebooting = 1;
    draw_splash_screen();
    usleep(100000);

    update_jailbreak();
    unlink("/var/db/sysstatuscheck_should_check");
    unlink("/var/db/mmaintenanced");
    unlink("/tmp/mmaintenanced");
    sync();
    
    sync_volume_np("/private/var", SYNC_VOLUME_FULLSYNC);
    sync_volume_np("/", SYNC_VOLUME_FULLSYNC);

    unmount("/Developer", MNT_FORCE);
    usleep(100000);
    sync();
}

static xpc_object_t jbserver_handle_xpc_message(xpc_object_t message) {
    if (message == NULL || xpc_get_type(message) != XPC_TYPE_DICTIONARY) return message;
    const char *name = xpc_dictionary_get_string(message, "name");
    if (name != NULL && strcmp(name, "com.staturnz.jbserver") == 0) {
        jbserver_handle_request(message, NULL);
        xpc_release(message);
        return NULL;
    }
    
    uint64_t routine = xpc_dictionary_get_uint64(message, "routine");
    if (routine == 821) {
        uint64_t subsystem = xpc_dictionary_get_uint64(message, "subsystem");
        if (subsystem == 3) {
            uint64_t type = xpc_dictionary_get_uint64(message, "type");
            uint64_t handle = xpc_dictionary_get_uint64(message, "handle");

            if (type == 1 && handle == 0) {
                uint64_t flags = xpc_dictionary_get_uint64(message, "flags");
                if ((flags & RB2_USERREBOOT) == RB2_USERREBOOT) {
                    jbserver_prepare_reboot();
                }
            }
        }
    }
    return message;
}

// ios 12 and earlier
static xpc_object_t xpc_serializer_unpack_hook(void *a1, void *a2, void *a3) {
    xpc_object_t message = xpc_serializer_unpack_orig(a1, a2, a3);
    if (message == NULL || kinfo->initialized == 0 || kinfo->userspace_rebooting == 1) {
        return message;
    }
    return jbserver_handle_xpc_message(message);
}

// ios 13 and later
static int xpc_receive_mach_msg_hook(void *a1, void *a2, void *a3, void *a4, xpc_object_t *out) {
    xpc_object_t message = NULL;
    int status = xpc_receive_mach_msg_orig(a1, a2, a3, a4, &message);
    if (message == NULL || kinfo->initialized == 0 || kinfo->userspace_rebooting == 1) {
        if (out != NULL) *out = message;
        return status;
    }

    xpc_object_t output = jbserver_handle_xpc_message(message);
    if (message != NULL && output != NULL && out != NULL) *out = output;
    return (output == NULL) ? EINVAL : status;
}

static int xpc_pipe_handle_mig_hook(void *a1, void *a2, void *a3) {
    jbserver_base_msg_t *msg = (jbserver_base_msg_t *)a1;
    if (msg != NULL && msg->hdr.msgh_id == JBSERVER_MSG_ID && msg->hdr.msgh_size >= sizeof(jbserver_base_msg_t)) {
        if (msg->magic == JBSERVER_MSG_MAGIC) {
            if (kinfo->initialized != 0 && kinfo->userspace_rebooting != 1) {
                jbserver_handle_request(NULL, msg);
            }
            return EINVAL;
        }
    }
    return xpc_pipe_handle_mig_orig(a1, a2, a3);
}

static xpc_object_t xpc_dictionary_get_value_hook(xpc_object_t dict, const char *key) {
    if (dict == NULL || key == NULL) return NULL;
    xpc_object_t value = xpc_dictionary_get_value_orig(dict, key);
    if (value == NULL) return NULL;

    if (strcmp(key, "LaunchDaemons") == 0 && xpc_get_type(value) == XPC_TYPE_DICTIONARY) {
        DIR *dir = opendir("/Library/LaunchDaemons");
        if (dir != NULL) {
            struct dirent *entry = NULL;
            char plist_path[PATH_MAX] = {0};

            while ((entry = readdir(dir)) != NULL) {
                char *item = (char *)entry->d_name;
                if (strstr(item, ".plist") == NULL || strstr(item, "jailbreakd") != NULL) continue;
                bzero(plist_path, PATH_MAX);
                snprintf(plist_path, PATH_MAX-1, "/Library/LaunchDaemons/%s", item);

                xpc_object_t plist = xpc_open_plist(plist_path);
                if (plist != NULL) {
                    xpc_dictionary_set_value(value, plist_path, plist);
                    xpc_release(plist);
                }
            }
            closedir(dir);
        }

        xpc_object_t misd_dict = xpc_dictionary_get_value_orig(value, "/System/Library/LaunchDaemons/com.apple.MobileInternetSharing.plist");
        if (misd_dict != NULL && xpc_get_type(misd_dict) == XPC_TYPE_DICTIONARY) {
            xpc_dictionary_set_bool(misd_dict, "KeepAlive", true);
        }
    } else if (strcmp(key, "Paths") == 0 && xpc_get_type(value) == XPC_TYPE_ARRAY) {
        xpc_object_t path_str = xpc_string_create("/Library/LaunchDaemons");
        if (path_str != NULL) {
            xpc_array_append_value(value, path_str);
            xpc_release(path_str);
        }
    }
    return value;
}

static int sandbox_check_by_audit_token_hook(audit_token_t at, const char *op, int filter, ...) {
	va_list va = NULL;
	va_start(va, filter);
    void *args[8] = {0};

    for (uint32_t i = 0; i < 8; i++) {
        args[i] = va_arg(va, void *);
    }
	va_end(va);

    if (op != NULL && (strcmp(op, "mach-lookup") == 0 || strcmp(op, "mach-register") == 0)) {
        char *service_name = (char *)(args[0]);

        if (service_name != NULL) {
            for (uint32_t i = 0; lookup_allow_list[i].name != NULL; i++) {
                if (strncmp(service_name, lookup_allow_list[i].name, lookup_allow_list[i].len) == 0) {
                    return 0;
                }
            }
        }
    }
    return sandbox_check_by_audit_token_orig(at, op, filter, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
}

int init_server(void) {
    void *handle = dlopen("/usr/lib/system/libxpc.dylib", RTLD_NOW);
    if (handle == NULL) handle = RTLD_DEFAULT;

    if (kinfo->version[0] <= 12) {
        void *symbol = dlsym(handle, "_xpc_serializer_unpack");
        if (symbol == NULL) return -1;

        int status = hook_function(symbol, (void *)xpc_serializer_unpack_hook, (void **)&xpc_serializer_unpack_orig);
        if (status != 0 || xpc_serializer_unpack_orig == NULL) return -1;
    } else {
        void *symbol = dlsym(handle, "xpc_receive_mach_msg");
        if (symbol == NULL) return -1;

        int status = hook_function(symbol, (void *)xpc_receive_mach_msg_hook, (void **)&xpc_receive_mach_msg_orig);
        if (status != 0 || xpc_receive_mach_msg_orig == NULL) return -1;
    }

    void *symbol = dlsym(handle, "_xpc_pipe_handle_mig");
    if (symbol == NULL) return -1;

    int status = hook_function(symbol, (void *)xpc_pipe_handle_mig_hook, (void **)&xpc_pipe_handle_mig_orig);
    if (status != 0 || xpc_pipe_handle_mig_orig == NULL) return -1;

    hook_function(sandbox_check_by_audit_token, (void *)sandbox_check_by_audit_token_hook, (void **)&sandbox_check_by_audit_token_orig);
    status = hook_function(xpc_dictionary_get_value, (void *)xpc_dictionary_get_value_hook, (void **)&xpc_dictionary_get_value_orig);
    return (status == 0 && xpc_dictionary_get_value_orig != NULL) ? 0 : -1;
}
