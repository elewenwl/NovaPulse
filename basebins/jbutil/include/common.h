#ifndef jbutil_common_h
#define jbutil_common_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <time.h>
#include <spawn.h>
#include <pthread.h>
#include <libgen.h>
#include <limits.h>
#include <_ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/dirent.h>
#include <mach/mach.h>
#include <mach/mach_traps.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <xpc/xpc.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <sys/attr.h>
#include <sys/fsgetpath.h>
#include <CoreFoundation/CoreFoundation.h>
#include <paths.h>
#include <compression.h>
#include <CommonCrypto/CommonDigest.h>

#define RB2_USERREBOOT 0x2000000000000000LLU

#ifndef CPU_SUBTYPE_PTRAUTH_ABI
#define CPU_SUBTYPE_PTRAUTH_ABI 0x80000000 
#endif

typedef struct section_64 section_t;
typedef struct segment_command_64 segment_command_t;
typedef struct mach_header_64 mach_header_t;
typedef struct nlist_64 nlist_t;

extern CFDictionaryRef _CFCopySystemVersionDictionary(void);
extern int reboot3(uint64_t flags);
extern uint32_t notify_post(const char *name);
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_name(int pid, void *buffer, uint32_t buffersize);
extern int csops(pid_t pid, unsigned int  ops, void *useraddr, size_t usersize);
extern char ***_NSGetArgv(void);
extern char ***_NSGetEnviron(void);
extern struct mach_header _mh_execute_header;

int userspace_reboot(void);

#endif /* jbutil_common_h */
