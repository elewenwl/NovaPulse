#include "basebin_hook.h"
#include "basebin_macho.h"
#include "util.h"
#include "jbserver.h"
#include "memory.h"
#include "trustcache.h"
#include "codesign.h"
#include "loader.h"

static orig_spawn_t orig_posix_spawn = NULL;
static orig_spawn_t orig_posix_spawnp = NULL;

static bool loader_blocked(const char *path, char **argv, posix_spawnattr_t *attr) {
    if (path == NULL || path[0] == '\0' || access("/usr/lib/base_hook.dylib", F_OK) != 0) return true;
    if (kinfo->initialized == 0 || kinfo->userspace_rebooting == 1) return true;
    if (strcmp(path, "/Developer/usr/bin/debugserver") == 0) return true;

    if (attr != NULL && *attr != NULL) {
        int proc_type = -1;
        posix_spawnattr_getprocesstype_np(attr, &proc_type);
        if (proc_type == POSIX_SPAWN_PROC_TYPE_DRIVER) return true;
    }

    if (strcmp(path, "/usr/libexec/xpcproxy") == 0) {
        if (argv != NULL && argv[0] != NULL && argv[1] != NULL) {
            char *bundle = argv[1];
            if (strstr(bundle, "debugserver") != NULL) return true;
            if (strstr(bundle, "com.apple") == NULL) return false;

            for (uint32_t i = 0; xpc_block_list[i] != NULL; i++) {
                if (strstr(bundle, xpc_block_list[i]) != NULL) return true;
            }
        }
    } else {
        for (uint32_t i = 0; path_block_list[i] != NULL; i++) {
            if (strcmp(path, path_block_list[i]) == 0) return true;
        }
    }
    return false;
}

int process_binary(const char *path) {
    if (strcmp(path, "/usr/libexec/xpcproxy") == 0) return 0;
    macho_ctx_t *macho = macho_load(path);
    if (macho == NULL) return -1;
    int status = 0;

    for (uint32_t i = 0; i < macho->slice_count; i++) {
        macho_slice_t *slice = &macho->slice_list[i];
        if (slice->file_type != MH_EXECUTE) continue;

        macho_signature_t *signature = macho_get_signature(slice);
        if (signature != NULL) {
            if (macho_should_sign(macho, signature)) {
                status = sign_binary((char *)path, signature->offset, signature->size, slice->offset, slice->file_type);
            } else {
                if (!trustcache_lock_check(signature->hash)) {
                    status = trustcache_lock_add_hash(signature->hash, signature->hash_type);
                }
            }
            macho_release_signature(signature);
        }
        if (status != 0) break;
    }

    macho_release(macho);
    return status;
}

static int spawn_handler(pid_t *pid, const char *path, posix_spawn_file_actions_t *file_actions, posix_spawnattr_t *attr, char **argv, char **env, orig_spawn_t orig_spawn) {
    if (orig_spawn == NULL) return -1;
    if (path != NULL && strcmp(path, "/sbin/launchd") == 0) {
        setenv("DYLD_INSERT_LIBRARIES", "/amethyst/launchd_hook.dylib", 1);
        setenv("AMETHYST", "1", 1);
        usleep(100000);
        return orig_spawn(pid, path, file_actions, attr, argv, environ);
    }

    if (kinfo->initialized == 0 && path != NULL && strcmp(path, "/usr/libexec/xpcproxy") == 0) {
        kinfo->initialized = 1;
    }

    if (loader_blocked(path, argv, attr)) {
        return orig_spawn(pid, path, file_actions, attr, argv, env);
    }

    posix_spawnattr_t target_attr = NULL;
    bool release_attr = false;
    if (attr != NULL && *attr != NULL) {
        target_attr = *attr;
    } else {
        posix_spawnattr_init(&target_attr);
        release_attr = true;
    }

    int16_t attr_flags = 0;
    posix_spawnattr_getflags(&target_attr, &attr_flags);
    attr_flags &= ~POSIX_SPAWN_SETEXEC;
    posix_spawnattr_setflags(&target_attr, attr_flags);

    process_binary(path);
    char **target_env = env_copy(env, 4);
    env_set(target_env, "DYLD_INSERT_LIBRARIES", "/usr/lib/base_hook.dylib", false);
    if (access("/amethyst/.disable_tweaks", F_OK) == 0) env_set(target_env, "DISABLE_TWEAKS", "1", false);
    if (kinfo->version[0] == 12 && kinfo->version[1] < 2) {
        env_set(target_env, "DYLD_FALLBACK_LIBRARY_PATH", "/usr/lib/libswift/stable", true);
    }

    if (target_attr != NULL) {
        ps_attr_t *ps_attr = (ps_attr_t *)target_attr;
        uint32_t multiplier = 2;

        if ((ps_attr->psa_jetsam_flags & POSIX_SPAWN_JETSAM_SET) != 0) {
            for (uint32_t i = 0; jetsam_list[i] != NULL; i++) {
                if (strstr(path, jetsam_list[i]) != NULL) {
                    multiplier = 3;
                    break;
                }
            }
            
            if (ps_attr->psa_memlimit_active != -1) {
               ps_attr->psa_memlimit_active = (ps_attr->psa_memlimit_active * multiplier);
            }

            if (ps_attr->psa_memlimit_inactive != -1) {
                ps_attr->psa_memlimit_inactive = (ps_attr->psa_memlimit_inactive * multiplier);
            }
        }
    }

    int status = orig_spawn(pid, path, file_actions, &target_attr, argv, target_env);
    if (release_attr) posix_spawnattr_destroy(&target_attr);
    env_release(target_env);
    return status;
}

static int posix_spawn_hook(pid_t *pid, const char *path, posix_spawn_file_actions_t *file_actions, posix_spawnattr_t *attr, char **argv, char **env) {
    return spawn_handler(pid, path, file_actions, attr, argv, env, orig_posix_spawn);
}

int init_loader(void) {
    hook_function((void *)posix_spawn, (void *)posix_spawn_hook, (void **)&orig_posix_spawn);
    return 0;
}
