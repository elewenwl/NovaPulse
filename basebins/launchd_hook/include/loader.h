#ifndef launchd_hook_loader_h
#define launchd_hook_loader_h

#include "info.h"
#include "codesign.h"

static const char *path_block_list[] = {
    "/usr/libexec/keybagd",
    "/usr/libexec/FinishRestoreFromBackup",
    "/usr/libexec/sysstatuscheck",
    "/usr/libexec/usermanagerd",
    "/usr/libexec/init_featureflags",
    "/usr/libexec/FinishRestoreFromBackup",
    "/usr/libexec/adprivacyd",
    NULL
};

static const char *xpc_block_list[] = {
    "com.apple.syslogd",
    "com.apple.logd",
    "com.apple.aslmanager",
    "com.apple.mobile.keybagd",
    "com.apple.notifyd",
    "com.apple.ReportMemoryException",
    "com.apple.GSSCred",
    "com.apple.UIKit.ShareUI",
    "com.apple.MTLCompilerService",
    NULL
};

static const char *jetsam_list[] = {
    "CommCenter",
    "backboardd",
    "SpringBoard",
    "locationd",
    "mediaserverd",
    "com.apple.siri.embeddedspeech",
    NULL
};

typedef int (*orig_spawn_t)(pid_t *, const char *, posix_spawn_file_actions_t *, posix_spawnattr_t *, char **, char **);

int process_binary(const char *path);
int init_loader(void);

#endif /* launchd_hook_loader_h */
