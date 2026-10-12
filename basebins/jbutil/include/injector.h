#ifndef jbutil_injector_h
#define jbutil_injector_h

#include "common.h"
#include "process.h"

#define PTHREAD_PATH "/usr/lib/system/libsystem_pthread.dylib"
#define PTHREAD_ALT_PATH "/usr/lib/libpthread.dylib"
#define LIBDYLD_PATH "/usr/lib/system/libdyld.dylib"
#define LIBSYSTEM_PATH "/usr/lib/libSystem.B.dylib"

typedef struct {
    process_ctx_t *process;
    thread_t thread;
    const char *path;
    uint16_t version[3];
    uint64_t pt_set_self_sym;
    uint64_t pt_create_mach_sym;
    uint64_t dlopen_sym;
    uint64_t spin_gadget;
    uint64_t stack_data;
    uint64_t string_data;
    bool disable_ptrauth;
    bool requires_rop_thread_data;
    bool requires_rop_thread_setup;
    bool already_injected;
} injector_ctx_t;

void injector_release(injector_ctx_t *ctx);
injector_ctx_t *injector_init(pid_t pid, const char *path);
int inject_dylib(pid_t pid, const char *path);

#endif /* jbutil_injector_h */
