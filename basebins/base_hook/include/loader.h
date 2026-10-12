#ifndef base_hook_loader_h
#define base_hook_loader_h

#include "basebin_common.h"

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

static const char *jetsam_list[] = {
    "CommCenter",
    "backboardd",
    "SpringBoard",
    "locationd",
    "mediaserverd",
    "com.apple.siri.embeddedspeech",
    NULL
};

void init_loader(void);

#endif /* base_hook_loader_h */
