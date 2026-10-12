#ifndef launchd_hook_jbserver_h
#define launchd_hook_jbserver_h

#include "info.h"
#include "codesign.h"

#define JBSERVER_MSG_ID     0x40000206
#define JBSERVER_MSG_MAGIC  0x48415858
#define P_SUGID 0x00000100

#define get_audit_token_pid(token) ((pid_t)(token.val[5]))
#define get_audit_token_uid(token) ((uid_t)(token.val[1]))
#define get_audit_token_ruid(token) ((uid_t)(token.val[3]))
#define get_audit_token_gid(token) ((gid_t)(token.val[2]))
#define get_audit_token_rgid(token) ((gid_t)(token.val[4]))
#define get_audit_token_idversion(token) ((gid_t)(token.val[7]))

typedef struct {
    const char *name;
    uint32_t len;
} lookup_info_t;

typedef struct {
	mach_msg_header_t hdr;
    uint32_t magic;
    pid_t pid;
    uint32_t cmd;
} jbserver_base_msg_t;

typedef struct {
    jbserver_base_msg_t base;
    uint32_t type;
    uint8_t hash[20];
} jbserver_trustcache_msg_t;

typedef struct {
    jbserver_base_msg_t base;
} jbserver_init_process_msg_t;

typedef struct {
	mach_msg_header_t hdr;
    uint32_t error;
    mach_msg_trailer_t trailer;
} jbserver_reply_msg_t;

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

extern mach_msg_header_t *dispatch_mach_msg_get_msg(void *, size_t *);
extern int xpc_pipe_routine_reply(xpc_object_t reply);

int init_server(void);

#endif /* launchd_hook_jbserver_h */
