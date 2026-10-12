#ifndef basebins_jbserver_h
#define basebins_jbserver_h

#include "basebin_common.h"

typedef enum {
    JBSERVER_CMD_UNKNOWN = 0,
    JBSERVER_CMD_TRUSTCACHE,
    JBSERVER_CMD_SIGN_BINARY,
    JBSERVER_CMD_INIT_PROCESS,
    JBSERVER_CMD_PLATFORMIZE,
    JBSERVER_CMD_UNSANDBOX,
    JBSERVER_CMD_SET_UID_GID,
    JBSERVER_CMD_HEARTBEAT
} jbserver_cmd_t;

typedef enum {
    JBSERVER_ERR_SUCCESS = 0,
    JBSERVER_ERR_INVALID_PROCESS,
    JBSERVER_ERR_INVALID_CD_HASH,
    JBSERVER_ERR_INVALID_PATH,
    JBSERVER_ERR_INVALID_REQUEST,
    JBSERVER_ERR_UNKNOWN_CMD,
    JBSERVER_ERR_TRUSTCACHE_FAILURE,
    JBSERVER_ERR_CLIENT_FAILURE,
    JBSERVER_ERR_SERVER_FAILURE,
    JBSERVER_ERR_UNKNOWN_FAILURE
} jbserver_err_t;

typedef enum {
    JBSERVER_UNSANDBOX_NONE = 0,
    JBSERVER_UNSANDBOX_EXT,
    JBSERVER_UNSANDBOX_FULL,
    JBSERVER_UNSANDBOX_AUTO,
} jbserver_unsandbox_type_t;

static const char *sb_none_override_list[] = {
    "PluginKitPlugin",
    ".appex",
    "XPCServices",
    NULL
};

static const char *sb_ext_override_list[] = {
    "/usr/libexec/securityd",
    "/usr/libexec/assertiond",
    "/usr/libexec/backboardd",
    "/usr/libexec/pkd",
    "/usr/libexec/splashboardd",
    "/usr/libexec/mediaserverd",
    "/System/Library/PrivateFrameworks/MobileContainerManager.framework/Support/containermanagerd",
    "/Application/MobileSafari.app/MobileSafari",
    "/Application/Web.app/Web",
    "/System/Library/CoreServices/SpringBoard.app/SpringBoard",
    NULL
};

static const char *sb_full_override_list[] = {
    "/Application/Sileo.app/Sileo",
    "/Application/Sileo.app/giveMeRoot",
    "/Application/Zebra.app/Zebra",
    "/Application/Zebra.app/supersling",
    NULL
};

jbserver_err_t jbserver_trustcache(uint8_t *cd_hash, uint8_t hash_type);
jbserver_err_t jbserver_sign_binary(const char *path, uint32_t le_offset, uint32_t le_size, uint32_t slice_offset, uint32_t file_type);
jbserver_err_t jbserver_init_process(pid_t pid, uid_t uid, gid_t gid, bool unsandbox);
jbserver_err_t jbserver_platformize(pid_t pid);
jbserver_err_t jbserver_unsandbox(pid_t pid, jbserver_unsandbox_type_t type);
jbserver_err_t jbserver_set_uid_gid(pid_t pid, uid_t uid, gid_t gid);
jbserver_err_t jbserver_heartbeat(void);
jbserver_err_t jbserver_process_binary(const char *path);

#endif /* basebins_jbserver_h */
