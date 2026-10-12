#include "injector.h"
#include "process.h"
#include "extract.h"
#include "install.h"
#include "ptrauth.h"
#include "util.h"
#include "dyld.h"
#include "tnsv2.h"

void print_usage(void) {
    fprintf(stderr, "usage: jbutil -[i:kURb:d:cu:r:ph]\n");
    fprintf(stderr, "   -i, --inject-dylib <pid> <dylib-path>\n");
    fprintf(stderr, "   -k, --kickstart\n");
    fprintf(stderr, "   -U, --userspace-reboot\n");
    fprintf(stderr, "   -R, --respring\n");
    fprintf(stderr, "   -b, --install-bootstrap <tar-path>\n");
    fprintf(stderr, "   -d, --install-deb <deb-path>\n");
    fprintf(stderr, "   -c, --refresh-icon-cache\n");
    fprintf(stderr, "   -u, --unregister-app <app-path>\n");
    fprintf(stderr, "   -r, --register-app <app-path>\n");
    fprintf(stderr, "   -h, --help\n");
}

int userspace_reboot(void) {
    process_get_root();
    unlink("/var/db/sysstatuscheck_should_check");
    unlink("/var/db/mmaintenanced");
    unlink("/tmp/mmaintenanced");
    sync();

    int status = reboot3(RB2_USERREBOOT);
    for (uint32_t i = 0; i < 250; i++) {
        usleep(1000);
        sync();
    }
    return status;
}

int main(int argc, char **argv) {
    int opt = -1;
    int idx = 0;

#if !defined(__arm64e__)
    if (strcmp(argv[0], "launchctl") == 0) {
        return tnsv2_startup();
    }
#endif

    struct option long_opts[] = {
        {"inject-dylib", required_argument, 0, 'i'},
        {"kickstart", no_argument, 0, 'k'},
        {"userspace-reboot", no_argument, 0, 'U'},
        {"respring", no_argument, 0, 'R'},
        {"install-bootstrap", required_argument, 0, 'b'},
        {"install-deb", required_argument, 0, 'd'},
        {"refresh-icon-cache", no_argument, 0, 'c'},
        {"unregister-app", required_argument, 0, 'u'},
        {"register-app", required_argument, 0, 'r'},
        {"patch-dyld", no_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    if (argc <= 1) {
        print_usage();
        return EINVAL;
    }

    while ((opt = getopt_long(argc, argv, "i:kURb:d:cu:r:ph", long_opts, &idx)) != -1) {
        switch (opt) {
            case 'i': {
                char *pid_str = optarg;
                pid_t pid = atoi(pid_str);
                if (pid <= 0 || optind >= argc) {
                    fprintf(stderr, "[-] invalid or missing pid\n");
                    return EINVAL;
                }

                char *dylib_path = argv[optind++]; 
                if (dylib_path == NULL || access(dylib_path, F_OK) != 0) {
                    fprintf(stderr, "[-] invalid or missing path\n");
                    return EINVAL;
                }
                return inject_dylib(pid, dylib_path);
            } break;

            case 'k': {
                return inject_dylib(1, "/amethyst/launchd_hook.dylib");
            } break;

            case 'U': {
                return userspace_reboot();
            } break;

            case 'R': {
                return respring_device();
            } break;

            case 'b': {
                char *tar_path = optarg;
                if (tar_path == NULL || access(tar_path, F_OK) != 0) {
                    fprintf(stderr, "[-] invalid or missing path\n");
                    return EINVAL;
                }
                return install_bootstrap(tar_path);
            } break;

            case 'd': {
                char *deb_path = optarg;
                if (deb_path == NULL || access(deb_path, F_OK) != 0) {
                    fprintf(stderr, "[-] invalid or missing path\n");
                    return EINVAL;
                }
                return install_deb(deb_path);
            } break;

            case 'c': {
                return refresh_icon_cache();
            } break;

            case 'u': {
                char *app_path = optarg;
                if (app_path == NULL || access(app_path, F_OK) != 0) {
                    fprintf(stderr, "[-] invalid or missing path\n");
                    return EINVAL;
                }
                return unregister_app(app_path);
            } break;

            case 'r': {
                char *app_path = optarg;
                if (app_path == NULL || access(app_path, F_OK) != 0) {
                    fprintf(stderr, "[-] invalid or missing path\n");
                    return EINVAL;
                }
                return register_app(app_path);
            } break;

            case 'p': {
                return patch_dyld();
            } break;

            default: {
                print_usage();
                return 0;
            }
        }
    }
    return -1;
}
