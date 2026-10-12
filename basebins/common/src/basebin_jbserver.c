#include "basebin_util.h"
#include "basebin_macho.h"
#include "basebin_jbserver.h"

static jbserver_err_t jbserver_send_msg_with_reply(xpc_object_t request, xpc_object_t *output) {
    struct xpc_global_data *global = NULL;
    if (_os_alloc_once_table[1].once == -1) {
        global = _os_alloc_once_table[1].ptr;
    } else {
        uint32_t num = (basebin_info.ios_version[0] == 13) ? 448 : 472;
        global = _os_alloc_once(&_os_alloc_once_table[1], num, NULL);
        if (global != NULL) _os_alloc_once_table[1].once = -1;
    }

    if (global == NULL) return JBSERVER_ERR_CLIENT_FAILURE;
    if (global->xpc_bootstrap_pipe == NULL) {
        mach_port_t *ports = NULL;
        mach_msg_type_number_t count = 0;

        if (mach_ports_lookup(mach_task_self(), &ports, &count) == 0) {
            if (MACH_PORT_VALID(ports[0])) {
                global->task_bootstrap_port = ports[0];
                global->xpc_bootstrap_pipe = xpc_pipe_create_from_port(global->task_bootstrap_port, 0);

                if (global->xpc_bootstrap_pipe == NULL) {
                    mach_port_deallocate(mach_task_self(), global->task_bootstrap_port);
                    global->task_bootstrap_port = MACH_PORT_NULL;
                }
            }

            for (uint32_t i = 1; i < count; i++) {
                if (MACH_PORT_VALID(ports[i])) {
                    mach_port_deallocate(mach_task_self(), ports[i]);
                }
            }
            vm_deallocate(mach_task_self(), (vm_address_t)ports, sizeof(mach_port_t) * count);
        }
    }

    if (global->xpc_bootstrap_pipe == NULL) return JBSERVER_ERR_CLIENT_FAILURE;
    xpc_object_t reply = NULL;

    xpc_pipe_routine(global->xpc_bootstrap_pipe, request, &reply);
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

jbserver_err_t jbserver_trustcache(uint8_t *cd_hash, uint8_t hash_type) {
    if (cd_hash == NULL) return JBSERVER_ERR_INVALID_REQUEST;
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_TRUSTCACHE);
    xpc_dictionary_set_data(request, "hash", cd_hash, 20);
    xpc_dictionary_set_uint64(request, "type", hash_type);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

jbserver_err_t jbserver_sign_binary(const char *path, uint32_t le_offset, uint32_t le_size, uint32_t slice_offset, uint32_t file_type) {
    if (path == NULL || le_size == 0) return JBSERVER_ERR_CLIENT_FAILURE;
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_SIGN_BINARY);

    xpc_object_t target_path = xpc_string_create(path);
    xpc_dictionary_set_value(request, "path", target_path);
    xpc_release(target_path);

    xpc_dictionary_set_uint64(request, "le_offset", (uint64_t)le_offset);
    xpc_dictionary_set_uint64(request, "le_size", (uint64_t)le_size);
    xpc_dictionary_set_uint64(request, "slice_offset", (uint64_t)slice_offset);
    xpc_dictionary_set_uint64(request, "file_type", (uint64_t)file_type);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

jbserver_err_t jbserver_init_process(pid_t pid, uid_t uid, gid_t gid, bool unsandbox) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_INIT_PROCESS);
    if (pid != getpid()) xpc_dictionary_set_int64(request, "pid", (int64_t)pid);
    if (uid != -1) xpc_dictionary_set_int64(request, "uid", (int64_t)uid);
    if (gid != -1) xpc_dictionary_set_int64(request, "gid", (int64_t)gid);
    if (unsandbox) xpc_dictionary_set_bool(request, "unsandbox", true);

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

static jbserver_unsandbox_type_t jbserver_get_unsandbox_type(pid_t pid) {
    if (pid != getpid()) return JBSERVER_UNSANDBOX_FULL;
    for (uint32_t i = 0; sb_none_override_list[i]; i++) {
        if (strstr(basebin_info.exec_path, sb_none_override_list[i]) != NULL) {
            return JBSERVER_UNSANDBOX_NONE;
        }
    }

    for (uint32_t i = 0; sb_ext_override_list[i]; i++) {
        if (strcmp(basebin_info.exec_path, sb_ext_override_list[i]) == 0) {
            return JBSERVER_UNSANDBOX_EXT;
        }
    }

    for (uint32_t i = 0; sb_full_override_list[i]; i++) {
        if (strcmp(basebin_info.exec_path, sb_full_override_list[i]) == 0) {
            return JBSERVER_UNSANDBOX_FULL;
        }
    }

    if (strncmp(basebin_info.exec_path, "/usr", strlen("/usr")) == 0 || 
        strncmp(basebin_info.exec_path, "/Library", strlen("/Library")) == 0 ||
        geteuid() == 0 || getegid() == 0) {
        return JBSERVER_UNSANDBOX_FULL;
    }

    xpc_object_t ents = xpc_load_entitlements(getpid());
    if (ents != NULL) {
        if (dict_get_bool(ents, "com.apple.private.security.sandbox") == 0 || 
            dict_get_bool(ents, "com.apple.private.security.no-sandbox") == 1 ||
            dict_get_bool(ents, "com.apple.private.security.container-required") == 0 ||
            dict_get_bool(ents, "com.apple.private.security.no-container") == 1 ||
            dict_get_bool(ents, "com.apple.private.skip-library-validation") == 1) {
            xpc_release(ents);
            return JBSERVER_UNSANDBOX_FULL;
        }

        xpc_object_t rw_list = dict_get_array(ents, "com.apple.security.exception.files.absolute-path.read-write");
        if (rw_list != NULL) {
            uint32_t count = xpc_array_get_count(rw_list);
            for (uint32_t i = 0; i < count; i++) {
                xpc_object_t value = xpc_array_get_value(rw_list, i);
                if (value == NULL || xpc_get_type(value) != XPC_TYPE_STRING) continue;

                const char *value_str = xpc_string_get_string_ptr(value);
                if (value_str == NULL) continue;
                
                if (strcmp(value_str, "/") == 0) {
                    xpc_release(ents);
                    return JBSERVER_UNSANDBOX_FULL;
                }
            }
        }

        xpc_object_t ro_list = dict_get_array(ents, "com.apple.security.exception.files.absolute-path.read-only");
        if (ro_list != NULL ) {
            uint32_t count = xpc_array_get_count(ro_list);
            for (uint32_t i = 0; i < count; i++) {
                xpc_object_t value = xpc_array_get_value(ro_list, i);
                if (value == NULL || xpc_get_type(value) != XPC_TYPE_STRING) continue;

                const char *value_str = xpc_string_get_string_ptr(value);
                if (value_str == NULL) continue;
                
                if (strcmp(value_str, "/") == 0) {
                    xpc_release(ents);
                    return JBSERVER_UNSANDBOX_FULL;
                }
            }
        }
        xpc_release(ents);
    }
    return JBSERVER_UNSANDBOX_EXT;
}

jbserver_err_t jbserver_unsandbox(pid_t pid, jbserver_unsandbox_type_t type) {
    if (type == JBSERVER_UNSANDBOX_NONE || sandbox_check(pid, NULL, 0, NULL) == 0) return JBSERVER_ERR_SUCCESS;
    if (type == JBSERVER_UNSANDBOX_AUTO) type = jbserver_get_unsandbox_type(pid);
    if (type == JBSERVER_UNSANDBOX_NONE) return JBSERVER_ERR_SUCCESS;

    if (type == JBSERVER_UNSANDBOX_EXT) {
        if (basebin_info.sb_ext_list == NULL) return JBSERVER_ERR_UNKNOWN_FAILURE;
        for (uint32_t i = 0; i < 6; i++) {
            char *ext = basebin_info.sb_ext_list + (i * 224);
            if (ext[0] == '\0') break;
            sandbox_extension_consume(ext);
        }
        return JBSERVER_ERR_SUCCESS;
    }

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

jbserver_err_t jbserver_heartbeat(void) {
    xpc_object_t request = jbserver_init_msg(JBSERVER_CMD_HEARTBEAT);
    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return (err == 0x1337) ? JBSERVER_ERR_SUCCESS : JBSERVER_ERR_SERVER_FAILURE;
}

jbserver_err_t jbserver_process_binary(const char *path) {
    if (path == NULL) return JBSERVER_ERR_INVALID_PATH;
    macho_ctx_t *macho = macho_load(path);
    if (macho == NULL) return JBSERVER_ERR_INVALID_PATH;
    jbserver_err_t status = JBSERVER_ERR_SUCCESS;

    for (uint32_t i = 0; i < macho->slice_count; i++) {
        macho_slice_t *slice = &macho->slice_list[i];
        if (slice->file_type != MH_EXECUTE) continue;

        macho_signature_t *signature = macho_get_signature(slice);
        if (signature != NULL) {
            if (macho_should_sign(macho, signature)) {
                status = jbserver_sign_binary(path, signature->offset, signature->size, slice->offset, slice->file_type);
            } else {
                status = jbserver_trustcache(signature->hash, signature->hash_type);
            }
            macho_release_signature(signature);
        }
        if (status != JBSERVER_ERR_SUCCESS) break;
    }
    
    macho_release(macho);
    return status;
}
