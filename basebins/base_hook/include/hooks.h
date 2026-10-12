#ifndef base_hook_hooks_h
#define base_hook_hooks_h

#include "basebin_common.h"

#define CS_VALID        0x00000001
#define CS_DEBUGGED     0x10000000
#define CS_OPS_STATUS   0x00000000
#define PT_ATTACH       10
#define PT_ATTACHEXC    14

typedef struct {
    const char *name;
    uint32_t len;
} lookup_info_t;

extern int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);
extern int csops_audittoken(pid_t pid, unsigned int ops, void *useraddr, size_t usersize, audit_token_t *token);

int init_hooks(void);

#endif /* base_hook_hooks_h */
