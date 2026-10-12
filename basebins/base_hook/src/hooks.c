#include "basebin_hook.h"
#include "basebin_util.h"
#include "basebin_jbserver.h"
#include "basebin_memory.h"
#include "hooks.h"

static int (*sandbox_check_by_audit_token_orig)(audit_token_t, const char *, int, ...) = NULL;

static lookup_info_t lookup_allow_list[] = {
    {"cy:", 3},
    {"lh:", 3},
    {"com.ex.substituted", 18},
    {"com.apple.ReportCrash.SimulateCrash", 35},
    {NULL, 0}
};

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

static int ptrace_hook(int request, pid_t pid, uint64_t addr, int data) {
    int status = sys_ptrace(request, pid, addr, data);
    if (status != 0 || pid <= 1 || pid == getpid()) return status;

    if (request == PT_ATTACH || request == PT_ATTACHEXC) {
        jbserver_init_process(pid, -1, -1, true);
    }
    return status;
}

static int csops_hook(pid_t pid, unsigned int ops, void *useraddr, size_t usersize) {
    int status = sys_csops(pid, ops, useraddr, usersize);
    if (status != 0 || ops != CS_OPS_STATUS || useraddr == NULL || usersize != 4) return status;

    uint32_t cs_flags = *(uint32_t *)useraddr;
    cs_flags |= CS_VALID | CS_DEBUGGED;
    *(uint32_t *)useraddr = cs_flags;
    return status;
}

static int csops_audittoken_hook(pid_t pid, unsigned int ops, void *useraddr, size_t usersize, audit_token_t *token) {
    int status = sys_csops_audittoken(pid, ops, useraddr, usersize, token);
    if (status != 0 || ops != CS_OPS_STATUS || useraddr == NULL || usersize != 4) return status;

    uint32_t cs_flags = *(uint32_t *)useraddr;
    cs_flags |= CS_VALID | CS_DEBUGGED;
    *(uint32_t *)useraddr = cs_flags;
    return status;
}

int init_hooks(void) {
    if (basebin_info.is_xpcproxy) {
        hook_function(sandbox_check_by_audit_token, (void *)sandbox_check_by_audit_token_hook, (void **)&sandbox_check_by_audit_token_orig);
        return 0;
    }

    // preload CoreFoundation and Foundation in dyld3 mode
    if (basebin_info.dyld3_mode) {
        dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW);
        dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_NOW);
        dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_NOW);
    }

    hook_function((void *)csops, (void *)csops_hook, NULL);
    hook_function((void *)csops_audittoken, (void *)csops_audittoken_hook, NULL);

    if (strcmp(basebin_info.proc_name, "debugserver") == 0) {
        hook_function(ptrace, (void *)ptrace_hook, NULL);
    }

    // fix issue where Foundation doesn't initialize correctly in dyld3 mode
    if (basebin_info.dyld3_mode) {
        if (basebin_info.dyld_handoff->ns_init_platform_symbol != 0) {
            void (*NSInitializePlatform)(void) = ptrauth_ia((void *)(basebin_info.dyld_handoff->ns_init_platform_symbol + basebin_info.dsc_slide), NULL);
            NSInitializePlatform();
        }
    }
    return 0;
}
