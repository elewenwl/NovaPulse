#include "basebin_common.h"
#include "basebin_util.h"
#include "basebin_hook.h"
#include "basebin_jbserver.h"
#include "basebin_macho.h"
#include "basebin_memory.h"
#include "loader.h"
#include "hooks.h"

int init_tweaks(void) {
    if (access("/.disable_tweakinject", F_OK) == 0 || getenv("DISABLE_TWEAKS") != NULL) return 0;
    if (access("/amethyst/.disable_tweaks", F_OK) == 0 || access("/var/mobile/.tweaks_disabled", F_OK) == 0) return 0;

    if (strstr(basebin_info.exec_path, "SpringBoard.app/SpringBoard") == NULL) {
        if (getenv("_SafeMode") || getenv("_MSSafeMode") || getenv("_LHSafeMode")) return 0;
        if (access("/var/mobile/.safemode", F_OK) == 0 || access("/var/mobile/.eksafemode", F_OK) == 0) return 0;
    }

    const char *loader_path = NULL;
    if (access("/usr/lib/libsonar.dylib", F_OK) == 0) {
        loader_path = "/usr/lib/libsonar.dylib";
        setenv("SONAR_LOADER", "1", 1);
    } else if (access("/usr/lib/TweakInject.dylib", F_OK) == 0) {
        loader_path = "/usr/lib/TweakInject.dylib";
    }

    if (loader_path == NULL) return 0;
    void *handle = dlopen(loader_path, RTLD_NOW);
    return (handle == NULL) ? -1 : 0;
}

__attribute__((constructor)) static void ctor(int argc, char **argv, char **envp, char **apple) {
    if (basebin_init(apple) != 0) return;
    if (env_exists(envp, "JBUTIL_SPAWN", "1")) {
        char ppid_name[PATH_MAX+1] = {0};
        proc_name(getppid(), ppid_name, PATH_MAX);

        if (strcmp(ppid_name, "jbutil") == 0) {
            jbserver_init_process(getpid(), 0, 0, true);
            setuid(0);
            setgid(0);
            
            init_loader();
            init_hooks();
            return;
        }
    }

    if (getenv("PATH") == NULL) {
        setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin", 1);
    }

    if (env_exists(envp, "DYLD_INSERT_LIBRARIES", "/usr/lib/base_hook.dylib")) {
        unsetenv("DYLD_INSERT_LIBRARIES");
    }

    if (basebin_info.is_xpcproxy) {
        init_hooks();
        init_loader();
        return;
    }

    struct stat st = {0};
    if (lstat(basebin_info.exec_path, &st) == 0) {
        if (S_ISREG(st.st_mode)) {
            uid_t target_uid = -1;
            uid_t target_gid = -1;

            if ((st.st_mode & S_ISUID) == S_ISUID) target_uid = st.st_uid;
            if ((st.st_mode & S_ISGID) == S_ISGID) target_gid = st.st_gid;

            if (target_uid != -1 || target_gid != -1) {
                jbserver_set_uid_gid(getpid(), target_uid, target_gid);
            }
        }
    }

    jbserver_unsandbox(getpid(), JBSERVER_UNSANDBOX_AUTO);
    if (init_hooks() != 0) return;
    init_loader();
    init_tweaks();
}
