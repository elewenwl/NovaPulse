#include "process.h"
#include "ptrauth.h"
#include "injector.h"

static int inject_via_rop(injector_ctx_t *ctx) {
    size_t path_len = strlen(ctx->path);
    if ((ctx->string_data = process_alloc(ctx->process, path_len+1)) == 0) return -1;
    if ((ctx->stack_data = process_alloc(ctx->process, 0x40000)) == 0) return -1;

    uint64_t thread_ptr = (ctx->stack_data + 0x4000);
    if (process_write(ctx->process, ctx->string_data, (void *)ctx->path, path_len+1) != 0) return -1;
    if (thread_create(ctx->process->task, &ctx->thread) != 0) return -1;

    if (ctx->process->is_64bit) {
        arm_thread_state64_t state = {};
        mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
        if (thread_get_state(ctx->thread, ARM_THREAD_STATE64, (thread_state_t)&state, &count) != 0) return -1;

#if defined(__arm64e__)
        if (ctx->process->arch == PROCESS_ARCH_ARM64E) {
            if (ctx->requires_rop_thread_setup) {
                if (ctx->disable_ptrauth) {
                    state.__opaque_pc = (void *)ctx->pt_set_self_sym;
                } else {
                    state.__opaque_pc = ptrauth_ia((void *)ctx->pt_set_self_sym, hash_discriminator("pc"));
                }
            } else {
                if (ctx->disable_ptrauth) {
                    state.__opaque_pc = (void *)ctx->pt_create_mach_sym;
                } else {
                    state.__opaque_pc = ptrauth_ia((void *)ctx->pt_create_mach_sym, hash_discriminator("pc"));
                }
            }

            if (ctx->disable_ptrauth) {
                state.__opaque_sp = (void *)(ctx->stack_data + 0x20000);
                state.__opaque_lr = (void *)ctx->spin_gadget;
            } else {
                state.__opaque_sp = ptrauth_da((void *)(ctx->stack_data + 0x20000), hash_discriminator("sp"));
                state.__opaque_lr = ptrauth_ia((void *)ctx->spin_gadget, hash_discriminator("lr"));
            }
        } else {
            if (ctx->requires_rop_thread_setup) {
                state.__opaque_pc = (void *)ctx->pt_set_self_sym;
            } else {
                state.__opaque_pc = (void *)ctx->pt_create_mach_sym;
            }
            
            state.__opaque_sp = (void *)(ctx->stack_data + 0x20000);
            state.__opaque_lr = (void *)ctx->spin_gadget;
        }
#else
        if (ctx->requires_rop_thread_setup) {
            state.__pc = ctx->pt_set_self_sym;
        } else {
            state.__pc = ctx->pt_create_mach_sym;
        }
        
        state.__sp = ctx->stack_data + 0x20000;
        state.__lr = ctx->spin_gadget;
#endif

        if (!ctx->requires_rop_thread_setup) {
            state.__x[0] = thread_ptr;
            state.__x[1] = 0;
            state.__x[2] = ctx->dlopen_sym;
            state.__x[3] = ctx->string_data;
#if defined(__arm64e__)
            if (ctx->process->arch == PROCESS_ARCH_ARM64E) {
                state.__x[2] = (uint64_t)ptrauth_ia((void *)ctx->dlopen_sym, NULL);
            }
#endif
        }

        if (ctx->requires_rop_thread_setup) {
            count = ARM_THREAD_STATE64_COUNT;
            if (ctx->process->arch == PROCESS_ARCH_ARM64E && ctx->disable_ptrauth) {
#if defined(__arm64e__)
                state.__opaque_flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
#endif
            }

            if (thread_set_state(ctx->thread, ARM_THREAD_STATE64, (thread_state_t)&state, count) != 0) return -1;
            if (thread_abort(ctx->thread) != 0) return -1;
            if (thread_resume(ctx->thread) != 0) return -1;
            usleep(10000);
            bool setup_done = false;

            for (uint32_t i = 0; i < 2000; i++) {
                if (thread_get_state(ctx->thread, ARM_THREAD_STATE64, (thread_state_t)&state, &count) != 0) return -1;
#if defined(__arm64e__)
                if ((uint64_t)xpaci(state.__opaque_pc) == ctx->spin_gadget)
#else
                if ((uint64_t)state.__pc == ctx->spin_gadget)
#endif
                {
                    thread_suspend(ctx->thread);
                    setup_done = true;
                    break;
                }
                usleep(1000);
            }

            if (!setup_done) return -1;
#if defined(__arm64e__)
            if (ctx->process->arch == PROCESS_ARCH_ARM64E && !ctx->disable_ptrauth) {
                state.__opaque_pc = ptrauth_ia((void *)ctx->dlopen_sym, hash_discriminator("pc"));
                state.__opaque_sp = ptrauth_da((void *)(ctx->stack_data + 0x20000), hash_discriminator("sp"));
                state.__opaque_lr = ptrauth_ia((void *)ctx->spin_gadget, hash_discriminator("lr"));
            } else {
                state.__opaque_pc = (void *)ctx->dlopen_sym;
                state.__opaque_sp = (void *)(ctx->stack_data + 0x20000);
                state.__opaque_lr = (void *)ctx->spin_gadget;
            }
#else
            state.__pc = ctx->dlopen_sym;
            state.__sp = ctx->stack_data + 0x20000;
            state.__lr = ctx->spin_gadget;
#endif

            state.__x[0] = ctx->string_data;
            state.__x[1] = RTLD_NOW;
            state.__x[2] = 0;
            state.__x[3] = 0;
        }

        count = ARM_THREAD_STATE64_COUNT;
        if (ctx->process->arch == PROCESS_ARCH_ARM64E && ctx->disable_ptrauth) {
#if defined(__arm64e__)
            state.__opaque_flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
#endif
        }

        if (thread_set_state(ctx->thread, ARM_THREAD_STATE64, (thread_state_t)&state, count) != 0) return -1;
        if (thread_abort(ctx->thread) != 0) return -1;
        if (thread_resume(ctx->thread) != 0) return -1;
        usleep(10000);

        for (uint32_t i = 0; i < 5000; i++) {
            if (thread_get_state(ctx->thread, ARM_THREAD_STATE64, (thread_state_t)&state, &count) != 0) return -1;
#if defined(__arm64e__)
            if ((uint64_t)xpaci(state.__opaque_pc) == ctx->spin_gadget)
#else
            if ((uint64_t)state.__pc == ctx->spin_gadget)
#endif
            {
                thread_suspend(ctx->thread);
                return 0;
            }
            usleep(1000);
        }
        thread_suspend(ctx->thread);
    }
    return -1;
}

void injector_release(injector_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (MACH_PORT_VALID(ctx->thread)) {
        thread_abort(ctx->thread);
        thread_terminate(ctx->thread);
    }

    if (ctx->stack_data != 0) process_dealloc(ctx->process, ctx->stack_data, 0x40000);
    if (ctx->string_data != 0) process_dealloc(ctx->process, ctx->string_data, strlen(ctx->path)+1);

    if (ctx->process != NULL) process_release(ctx->process);
    bzero(ctx, sizeof(injector_ctx_t));
    free(ctx);
}

injector_ctx_t *injector_init(pid_t pid, const char *path) {
    if (pid <= 0 || path == NULL) return NULL;
    injector_ctx_t *ctx = calloc(1, sizeof(injector_ctx_t));
    if (ctx == NULL) return NULL;
    int status = -1;

    CFDictionaryRef sys_ver = _CFCopySystemVersionDictionary();
    if (sys_ver == NULL) goto done;

    CFStringRef str = (CFStringRef)CFDictionaryGetValue(sys_ver, CFSTR("ProductVersion"));
    if (str == NULL) goto done;
    char buf[128] = {0};

    CFStringGetCString(str, buf, sizeof(buf)-1, kCFStringEncodingUTF8);
    CFRelease(sys_ver);
    
    sscanf(buf, "%hu.%hu.%hu", &ctx->version[0], &ctx->version[1], &ctx->version[2]);
    if (ctx->version[0] == 0) goto done;
    ctx->path = path;

    if ((ctx->process = process_init(pid)) == NULL) goto done;
    if (process_find_image(ctx->process, path) != 0) {
        ctx->already_injected = true;
        return ctx;
    }

    uintptr_t libdyld_image = process_find_image(ctx->process, LIBDYLD_PATH);
    if (libdyld_image == 0) libdyld_image = process_find_image(ctx->process, LIBSYSTEM_PATH);
    if (libdyld_image == 0) goto done;
    if ((ctx->dlopen_sym = process_find_symbol(ctx->process, libdyld_image, "_dlopen")) == 0) goto done;

    uintptr_t pthread_image = process_find_image(ctx->process, PTHREAD_PATH);
    if (pthread_image == 0) pthread_image = process_find_image(ctx->process, PTHREAD_ALT_PATH);
    if (pthread_image == 0) pthread_image = process_find_image(ctx->process, LIBSYSTEM_PATH);
    if (pthread_image == 0) goto done;

    ctx->pt_set_self_sym = process_find_symbol(ctx->process, pthread_image, "_pthread_set_self");
    if (ctx->pt_set_self_sym == 0) ctx->pt_set_self_sym = process_find_symbol(ctx->process, pthread_image, "__pthread_set_self");
    if (ctx->pt_set_self_sym == 0) ctx->pt_set_self_sym = process_find_symbol(ctx->process, pthread_image, "___pthread_set_self");
    if (ctx->pt_set_self_sym == 0) goto done;

    ctx->pt_create_mach_sym = process_find_symbol(ctx->process, pthread_image, "_pthread_create_from_mach_thread");
    if (ctx->pt_create_mach_sym == 0) ctx->pt_create_mach_sym = process_find_symbol(ctx->process, pthread_image, "__pthread_create_from_mach_thread");
    if (ctx->pt_create_mach_sym == 0) ctx->pt_create_mach_sym = process_find_symbol(ctx->process, pthread_image, "___pthread_create_from_mach_thread");

    ctx->spin_gadget = process_find_spin_gadget(ctx->process);
    ctx->disable_ptrauth = (ctx->process->arch != PROCESS_ARCH_ARM64E);

#if defined(__arm64e__)
    if (ctx->version[0] == 12 && ctx->process->arch == PROCESS_ARCH_ARM64E) {
        uint32_t cpu_family = 0;
        size_t size = sizeof(cpu_family);
        sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);

        if (cpu_family == CPUFAMILY_ARM_VORTEX_TEMPEST) {
            thread_t test_thread = THREAD_NULL;
            thread_create(mach_task_self(), &test_thread);

            if (!MACH_PORT_VALID(test_thread)) goto done;
            arm_thread_state64_t test_state = {};
            mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;

            int kr = thread_get_state(test_thread, ARM_THREAD_STATE64, (thread_state_t)&test_state, &count);
            thread_terminate(test_thread);
            if (kr != 0) goto done;
            ctx->disable_ptrauth = (test_state.__opaque_flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH);
        }
    }
#endif

    ctx->requires_rop_thread_data = (ctx->pt_create_mach_sym != 0);
    ctx->requires_rop_thread_setup = (!ctx->requires_rop_thread_data);
    status = 0;

done:
    if (status == 0) return ctx;
    injector_release(ctx);
    return NULL;
}

int inject_dylib(pid_t pid, const char *path) {
    injector_ctx_t *ctx = injector_init(pid, path);
    if (ctx == NULL) return -1;
    int status = -1;

    if (ctx->already_injected) {
        injector_release(ctx);
        return 0;
    }
    
    status = inject_via_rop(ctx);
    injector_release(ctx);
    return status;
}
