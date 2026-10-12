#include "common.h"

static jb_connection_t jb_connection = NULL;
static uint32_t ios_version = -1;

static uint32_t get_ios_version(void) {
    if (ios_version != -1) return ios_version;
    char version_str[64] = {0};
    size_t version_size = sizeof(version_str)-1;
    
    sysctlbyname("kern.osproductversion", version_str, &version_size, 0, 0);
    if (version_str[0] != '\0') {
        uint32_t version[3] = {0};
        sscanf(version_str, "%u.%u.%u", &version[0], &version[1], &version[2]);

        if (version[0] != 0) {
            ios_version = version[0];
            return ios_version;
        }
    }

    bzero(version_str, sizeof(version_str));
    version_size = sizeof(version_str)-1;
    sysctlbyname("kern.osrelease", version_str, &version_size, 0, 0);
    if (version_str[0] != '\0') {
        uint32_t version[3] = {0};
        sscanf(version_str, "%u.%u.%u", &version[0], &version[1], &version[2]);

        switch (version[0]) {
            case 19: ios_version = 13; break;
            case 18: ios_version = 12; break;
            default: ios_version = 0; break;
        }
        return ios_version;
    }

    ios_version = 0;
    return 0;
}

static jbserver_err_t jbserver_send_msg_with_reply(xpc_object_t request, xpc_object_t *output) {
    struct xpc_global_data *global = NULL;
    if (_os_alloc_once_table[1].once == -1) {
        global = _os_alloc_once_table[1].ptr;
    } else {
        uint32_t num = (get_ios_version() == 13) ? 448 : 472;
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
    xpc_dictionary_set_bool(request, "legacy", true);

    jbserver_err_t err = jbserver_send_msg(request);
    xpc_release(request);
    return err;
}

BASEBIN_EXPORT void jb_oneshot_fix_setuid_now(pid_t pid) {
    if (getuid() != 0 || getgid() != 0) jbserver_set_uid_gid(pid, 0, 0);
}

BASEBIN_EXPORT void jb_fix_setuid_now(jb_connection_t connection, pid_t pid) {
    if (getuid() != 0 || getgid() != 0) jbserver_set_uid_gid(pid, 0, 0);
}

BASEBIN_EXPORT void jb_fix_setuid(jb_connection_t connection, pid_t pid, jb_callback_t done) {
    if (getuid() != 0 || getgid() != 0) jbserver_set_uid_gid(pid, 0, 0);
    if (done != NULL) done(1);
}

BASEBIN_EXPORT void jb_oneshot_entitle_now(pid_t pid, uint32_t flags) {
    if (flags == 0 || (flags & FLAG_PLATFORMIZE)) jbserver_platformize(pid);
    if (flags == 0 || flags & FLAG_SANDBOX) jbserver_unsandbox(getpid());
    if ((flags & FLAG_WAIT_EXEC) || (flags & FLAG_DELAY)) usleep(500000);
    if (flags & FLAG_SIGCONT) kill(pid, SIGCONT);
}

BASEBIN_EXPORT void jb_entitle_now(jb_connection_t connection, pid_t pid, uint32_t flags) {
    if (flags == 0 || (flags & FLAG_PLATFORMIZE)) jbserver_platformize(pid);
    if (flags == 0 || flags & FLAG_SANDBOX) jbserver_unsandbox(getpid());
    if ((flags & FLAG_WAIT_EXEC) || (flags & FLAG_DELAY)) usleep(500000);
    if (flags & FLAG_SIGCONT) kill(pid, SIGCONT);
}

BASEBIN_EXPORT void jb_entitle(jb_connection_t connection, pid_t pid, uint32_t flags, jb_callback_t done) {
    if (flags == 0 || (flags & FLAG_PLATFORMIZE)) jbserver_platformize(pid);
    if (flags == 0 || flags & FLAG_SANDBOX) jbserver_unsandbox(getpid());
    if ((flags & FLAG_WAIT_EXEC) || (flags & FLAG_DELAY)) usleep(500000);
    if (flags & FLAG_SIGCONT) kill(pid, SIGCONT);
    if (done != NULL) done(1);
}

BASEBIN_EXPORT jb_connection_t jb_connect(void) {
    return &jb_connection;
}

BASEBIN_EXPORT void jb_disconnect(jb_connection_t connection) {
    return;
}
