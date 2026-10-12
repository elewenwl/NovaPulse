#include "process.h"
#include "util.h"

static uint32_t is_arm64e = -1;

int run_command(const char *path, char **args) {
    char *env[] = {
        "DYLD_INSERT_LIBRARIES=/usr/lib/base_hook.dylib", 
        "PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/lib:/usr/local/lib:/lib:/usr/libexec:/usr/lib/bash",
        "JBUTIL_SPAWN=1", 
        NULL,
        NULL
    };

    if (strstr(path, "dpkg") != NULL || strstr(path, "apt") != NULL) env[3] = "SILEO=1";
    pid_t pid = -1;
    int status = 0;
    int err = posix_spawn(&pid, path, NULL, NULL, args, env);
    if (err != 0 || pid == -1) return -1;
    
    do { if (waitpid(pid, &status, 0) == -1) return -1; }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return status;
}

static int device_init(void) {
    dlopen("/System/Library/PrivateFrameworks/MobileContainerManager.framework/MobileContainerManager", RTLD_NOW);
    dlopen("/System/Library/Frameworks/MobileCoreServices.framework/MobileCoreServices", RTLD_NOW);
    dlopen("/System/Library/PrivateFrameworks/SpringBoardServices.framework/SpringBoardServices", RTLD_NOW);
    dlopen("/System/Library/PrivateFrameworks/FrontBoardServices.framework/FrontBoardServices", RTLD_NOW);
    dlopen("/System/Library/PrivateFrameworks/BaseBoard.framework/BaseBoard", RTLD_NOW);
    return 0;
}

static id ls_get_workspace(void) {
    Class cls = objc_getClass("LSApplicationWorkspace");
    if (cls == NULL) return NULL;
    return ((id (*)(Class, SEL))objc_msgSend)(cls, sel_getUid("defaultWorkspace"));
}

static CFDictionaryRef open_plist(const char *path, bool mutable) {
    if (path == NULL) return NULL;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    uint32_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    const uint8_t *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return NULL;

    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, mapped, size, kCFAllocatorNull);
    CFOptionFlags options = mutable ? (kCFPropertyListMutableContainers | kCFPropertyListMutableContainersAndLeaves) : kCFPropertyListImmutable;
    CFDictionaryRef plist = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data, options, NULL, NULL);
    munmap((void *)mapped, size);
    CFRelease(data);
    return plist;
}

static CFStringRef get_bundle_id(const char *path) {
    if (path == NULL) return NULL;
    char plist_path[PATH_MAX] = {0};
    snprintf(plist_path, PATH_MAX-1, "%s/Info.plist", path);
    int fd = open(plist_path, O_RDONLY);
    if (fd < 0) return NULL;

    uint32_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    const uint8_t *mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return NULL;

    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, mapped, size, kCFAllocatorNull);
    CFDictionaryRef plist = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
    munmap((void *)mapped, size);
    CFRelease(data);

    if (plist == NULL) return NULL;
    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return NULL;
    }

    CFStringRef bundle_id = NULL;
    CFStringRef temp = CFDictionaryGetValue(plist, CFSTR("CFBundleIdentifier"));
    if (temp != NULL) bundle_id = CFStringCreateMutableCopy(NULL, 0, temp);
    
    CFRelease(plist);
    return bundle_id;
}

static void install_notify(void) {
    usleep(100000);
    notify_post("com.apple.mobile.application_installed");
    usleep(100000);
    sync();
}

int register_app(const char *path) {
    if (path == NULL || device_init() != 0) return -1;
    id workspace = ls_get_workspace();
    if (workspace == NULL) return -1;
    
    CFStringRef bundle_id = get_bundle_id(path);
    if (bundle_id == NULL) return -1;
    bool status = false;

    char resolved_path[PATH_MAX] = {0};
    char plugins_path[PATH_MAX] = {0};
    if (realpath(path, resolved_path) == NULL) {
        CFRelease(bundle_id);
        return -1;
    }

    Class cls = objc_getClass("MCMAppDataContainer");
    SEL sel = sel_getUid("containerWithIdentifier:createIfNecessary:existed:error:");
    id container = ((id (*)(Class, SEL, CFStringRef, bool, void *, void *))objc_msgSend)(cls, sel, bundle_id, true, NULL, NULL);
    if (container == NULL) {
        CFRelease(bundle_id);
        return -1;
    }

    id container_url = ((id (*)(id, SEL))objc_msgSend)(container, sel_getUid("url"));
    CFStringRef container_path = ((CFStringRef (*)(id, SEL))objc_msgSend)(container_url, sel_getUid("path"));
    CFStringRef cf_path = CFStringCreateWithCString(NULL, resolved_path, kCFStringEncodingUTF8);

    CFMutableDictionaryRef app_info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(app_info, CFSTR("ApplicationType"), CFSTR("System"));
    CFDictionarySetValue(app_info, CFSTR("BundleNameIsLocalized"), kCFBooleanTrue);
    CFDictionarySetValue(app_info, CFSTR("CFBundleIdentifier"), bundle_id);
    CFDictionarySetValue(app_info, CFSTR("CompatibilityState"), kCFBooleanFalse);
    CFDictionarySetValue(app_info, CFSTR("Container"), container_path);
    CFDictionarySetValue(app_info, CFSTR("IsDeletable"), kCFBooleanFalse);
    CFDictionarySetValue(app_info, CFSTR("Path"), cf_path);
    CFRelease(cf_path);

    snprintf(plugins_path, PATH_MAX-1, "%s/PlugIns", resolved_path);    
    CFMutableDictionaryRef plugin_list = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    cls = objc_getClass("MCMPluginKitPluginDataContainer");
    if (cls != NULL && access(plugins_path, F_OK) == 0) {
        DIR *dir = opendir(plugins_path);

        if (dir != NULL) {
            struct dirent *entry = NULL;

            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                char current_plugin[PATH_MAX] = {0};
                snprintf(current_plugin, PATH_MAX-1, "%s/%s", plugins_path, entry->d_name);

                CFStringRef plugin_bundle_id = get_bundle_id(current_plugin);
                if (plugin_bundle_id == NULL) continue;

                SEL sel = sel_getUid("containerWithIdentifier:error:");
                id plugin_container = ((id (*)(Class, SEL, CFStringRef, void *))objc_msgSend)(cls, sel, plugin_bundle_id, NULL);
                if (plugin_container == NULL) {
                    CFRelease(plugin_bundle_id);
                    continue;
                }

                id plugin_container_url = ((id (*)(id, SEL))objc_msgSend)(plugin_container, sel_getUid("url"));
                CFStringRef plugin_container_path = ((CFStringRef (*)(id, SEL))objc_msgSend)(plugin_container_url, sel_getUid("path"));
                if (plugin_container_path == NULL) {
                    CFRelease(plugin_bundle_id);
                    continue;
                }

                CFStringRef cf_path = CFStringCreateWithCString(NULL, current_plugin, kCFStringEncodingUTF8);
                CFMutableDictionaryRef plugin_info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(plugin_info, CFSTR("ApplicationType"), CFSTR("PluginKitPlugin"));
                CFDictionarySetValue(plugin_info, CFSTR("BundleNameIsLocalized"), kCFBooleanTrue);
                CFDictionarySetValue(plugin_info, CFSTR("CFBundleIdentifier"), plugin_bundle_id);
                CFDictionarySetValue(plugin_info, CFSTR("CompatibilityState"), kCFBooleanFalse);
                CFDictionarySetValue(plugin_info, CFSTR("Container"), plugin_container_path);
                CFDictionarySetValue(plugin_info, CFSTR("Path"), cf_path);
                CFDictionarySetValue(plugin_info, CFSTR("PluginOwnerBundleID"), bundle_id);

                CFDictionarySetValue(plugin_list, plugin_bundle_id, plugin_info);
                CFRelease(plugin_bundle_id);
                CFRelease(plugin_info);
                CFRelease(cf_path);
            }
        }
    }

    CFDictionarySetValue(app_info, CFSTR("_LSBundlePlugins"), plugin_list);
    sel = sel_getUid("registerApplicationDictionary:");
    status = ((bool (*)(id, SEL, CFDictionaryRef))objc_msgSend)(workspace, sel, app_info);
    CFRelease(plugin_list);
    CFRelease(app_info);

    CFRelease(bundle_id);
    install_notify();
    return status ? 0 : -1;
}

int unregister_app(const char *path) {
    if (path == NULL || device_init() != 0) return -1;
    id workspace = ls_get_workspace();
    if (workspace == NULL) return -1;

    CFStringRef cf_path = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (cf_path == NULL) return -1;

    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, cf_path, kCFURLPOSIXPathStyle, false);

    SEL sel = sel_getUid("unregisterApplication:");
    bool status = ((bool (*)(id, SEL, CFURLRef))objc_msgSend)(workspace, sel, url);

    CFRelease(cf_path);
    CFRelease(url);
    install_notify();
    return status ? 0 : -1;
}

int refresh_icon_cache(void) {
    if (device_init() != 0) return -1;
    id workspace = ls_get_workspace();
    if (workspace == NULL) return -1;

    SEL sel = sel_getUid("_LSPrivateRebuildApplicationDatabasesForSystemApps:internal:user:");
    bool status = ((bool (*)(id, SEL, bool, bool, bool))objc_msgSend)(workspace, sel, 1, 1, 0);
    return status ? 0 : -1;
}

int respring_device(void) {
    pid_t springboard_pid = process_get_pid("SpringBoard");
    pid_t backboardd_pid = process_get_pid("backboardd");

    int status = process_send_signal(springboard_pid, SIGTERM);
    if (status != 0) status = process_send_signal(backboardd_pid, SIGTERM);
    return status;
}

bool soc_is_arm64e(void) {
    if (is_arm64e == -1) {
#ifdef __arm64e__
        is_arm64e = 1;
        return true;
#endif
        uint32_t cpu_family = 0;
        size_t size = sizeof(cpu_family);
        sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);
    
        if (cpu_family == CPUFAMILY_ARM_VORTEX_TEMPEST || cpu_family == CPUFAMILY_ARM_LIGHTNING_THUNDER) {
            is_arm64e = 1;
        } else {
            is_arm64e = 0;
        }
    }
    return (is_arm64e == 1);
}

int get_ios_version(uint32_t *version) {
    CFDictionaryRef sys_ver = _CFCopySystemVersionDictionary();
    if (sys_ver == NULL) return -1;

    CFStringRef str = (CFStringRef)CFDictionaryGetValue(sys_ver, CFSTR("ProductVersion"));
    if (str == NULL) return -1;
    char buf[128] = {0};

    CFStringGetCString(str, buf, sizeof(buf)-1, kCFStringEncodingUTF8);
    CFRelease(sys_ver);
    
    sscanf(buf, "%u.%u.%u", &version[0], &version[1], &version[2]);
    return 0;
}

uint32_t get_page_size(void) {
    uint32_t cpu_family = 0;
    size_t size = sizeof(cpu_family);
    sysctlbyname("hw.cpufamily", &cpu_family, &size, NULL, 0);

    if (cpu_family == CPUFAMILY_ARM_CYCLONE) return 0x1000;
    if (cpu_family == CPUFAMILY_ARM_TYPHOON) {
        char board[64] = {0};
        size_t board_size = sizeof(board);
        sysctlbyname("hw.targettype", &board[0], &board_size, NULL, 0);
        if (strcasecmp(board, "J81AP") == 0 || strcasecmp(board, "J82AP") == 0) return 0x4000;
        return 0x1000;
    }
    return 0x4000;
}

const char *resolve_binary_name(const char *path) {
    if (path == NULL || path[0] == '\0') return NULL;
    size_t path_len = strlen(path);
    if (path_len <= 1 || path_len > PATH_MAX) return NULL;

    uint32_t idx = (uint32_t)path_len;
    for (; path[idx] != '/' && idx > 0; idx--);
    
    if (idx == 0 || idx == path_len) return NULL;
    return (const char *)&path[idx+1];
}

void *map_file(const char *path, uint32_t *size, bool write) {
    if (path == NULL || size == NULL) return NULL;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st = {0};
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return NULL;
    }

    uint32_t prot = PROT_READ | (write ? PROT_WRITE : PROT_NONE);
    void *data = mmap(NULL, st.st_size, prot, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) return NULL;
    *size = (uint32_t)st.st_size;
    return data;
}

void unmap_file(void *data, uint32_t size) {
    if (data == NULL || size == 0) return;
    munmap(data, size);
}

int copy_file(const char *src, const char *dest) {
    int fd = open(src, O_RDONLY);
    if (fd == -1) return -1;
    
    if (access(dest, R_OK) == 0) {
        unlink(dest);
        sync();
    }
    
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return -1;
    
    FILE *output = fopen(dest, "wb");
    if (output == NULL) {
        munmap(data, size);
        return -1;
    }

    fwrite(data, size, 1, output);
    munmap(data, size);
    fclose(output);
    sync();
    return access(dest, F_OK);
}
