#ifndef basebin_hook_h
#define basebin_hook_h

#include "basebin_common.h"
#include "basebin_instructions.h"

typedef enum {
    HOOK_TYPE_NONE = -1,
    HOOK_TYPE_DIRECT_BRANCH,
    HOOK_TYPE_ADRP_BR,
    HOOK_TYPE_ADRP_ADD_BR,
    HOOK_TYPE_LDR_BR,
    HOOK_TYPE_MOV2_BR,
    HOOK_TYPE_ALT_MOV2_BR,
    HOOK_TYPE_MOV3_BR,
    HOOK_TYPE_MOV4_BR,
} hook_type_t;

typedef struct {
    hook_type_t type;
    uint32_t size;
    bool requires_relocation;
} hook_info_t;

int hook_function(void *target, void *replacement, void **original);

#endif /* basebin_hook_h */
