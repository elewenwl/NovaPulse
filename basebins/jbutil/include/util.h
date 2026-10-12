#ifndef jbutil_util_h
#define jbutil_util_h

#include "common.h"

int run_command(const char *path, char **args);
int register_app(const char *path);
int unregister_app(const char *path);
int refresh_icon_cache(void);
int respring_device(void);
bool soc_is_arm64e(void);
int get_ios_version(uint32_t *version);
uint32_t get_page_size(void);
const char *resolve_binary_name(const char *path);
void *map_file(const char *path, uint32_t *size, bool write);
void unmap_file(void *data, uint32_t size);
int copy_file(const char *src, const char *dest);

#endif /* jbutil_util_h */
