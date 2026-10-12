#include "extract.h"

static mode_t parse_mode(uint8_t *data) {
    char *end = NULL;
    return (mode_t)strtol((char *)data, &end, 8);
}

static int parse_octal(uint8_t *data, size_t size) {
    int octal = 0;
    while ((data[0] < '0' || data[0] > '7') && size > 0) {
        data++;
        size--;
    }
    
    while (data[0] >= '0' && data[0] <= '7' && size > 0) {
        octal *= 8;
        octal += data[0] - '0';
        data++;
        size--;
    }
    return octal;
}

static time_t parse_time(uint8_t *data) {
    size_t size = 12;
    int64_t time = 0;
    while ((data[0] < '0' || data[0] > '7') && size > 0) {
        data++;
        size--;
    }
    
    while (data[0] >= '0' && data[0] <= '7' && size > 0) {
        time *= 8;
        time += data[0] - '0';
        data++;
        size--;
    }
    return (time_t)time;
}

static int set_time(uint8_t *data, time_t mtime, bool is_link) {
    struct timeval val[2] = {};
    val[0].tv_sec = mtime;
    val[0].tv_usec = (__darwin_suseconds_t)0;
    val[1].tv_sec = mtime;
    val[1].tv_usec = (__darwin_suseconds_t)0;

    if (is_link) return lutimes((char *)data, (void *)&val);
    return utimes((char *)data, (void *)&val);
}

static bool end_of_file(uint8_t *data) {
    for (int i = 511; i >= 0; i--) {
        if (data[i] != '\0') return false;
    }
    return true;
}

static bool verify_checksum(uint8_t *data) {
    int unaligned = 0;
    for (int i = 0; i < 512; i++) {
        if (i < 148 || i > 155) unaligned += data[i];
        else unaligned += 0x20;
    }
    return (unaligned == parse_octal(data + 148, 8));
}

static void create_directory(uint8_t *data, mode_t mode, uid_t uid, gid_t gid) {
    char *path = (char *)data;
    size_t path_size = strlen(path);
    if (path[path_size-1] == '/') path[path_size-1] = '\0';
    
    int rv = -1;
    if (access(path, F_OK) != 0) {
        rv = mkdir(path, mode);
        chown(path, uid, gid);
    }

    if (rv != 0) {
        char *ptr = strrchr(path, '/');
        if (ptr != NULL) {
            ptr[0] = '\0';
            create_directory(data, mode, uid, gid);
            ptr[0] = '/';
            
            if (access(path, F_OK) != 0) {
                mkdir(path, mode);
                chown(path, uid, gid);
            }
        }
    }
}

static FILE *create_file(uint8_t *data, mode_t mode, int owner, int group) {
    char *path = (char *)data;
    FILE *file = fopen(path, "wb+");
    
    if (file == NULL) {
        char *ptr = strrchr(path, '/');
        if (ptr != NULL) {
            ptr[0] = '\0';
            create_directory(data, mode, owner, group);
            ptr[0] = '/';
            file = fopen(path, "wb+");
        }
    }
    return file;
}

static int create_link(uint8_t *data, bool is_symlink) {
    char linkname[100] = {0};
    for (int i = 0; i < 100; i++) linkname[i] = data[157 + i];

    if (is_symlink) {
        if (symlink(linkname, (char *)data) != 0) return -1;

    } else {
        if (link(linkname, (char *)data) != 0) return -1;
    }
    set_time(data, parse_time(data + TAR_MTIME_OFF), 1);
    return 0;
}

int extract_tar(FILE *input, const char *path) {
    if (chdir(path) != 0) return -1;
    uint8_t data[512] = {0};
    FILE *file = NULL;
    size_t bytes_read = 0;
    int file_size = 0;
    char path_name[100] = {0};
    mode_t mode = 0;

    while (true) {
        bytes_read = fread(data, 1, 512, input);
        if (bytes_read < 512) return -1;
        if (end_of_file(data)) return 0;
        if (!verify_checksum(data)) return -1;
        
        file_size = parse_octal(data + 124, 12);
        char entry_type = (char)data[156];
        
        switch (entry_type) {
            case TAR_LNKTYPE: create_link(data, false); break;
            case TAR_SYMTYPE: create_link(data, true); break;
            case TAR_CHRTYPE: break;
            case TAR_BLKTYPE: break;
            case TAR_DIRTYPE:
                mode = parse_mode(data + TAR_OCTAL_OFF);
                create_directory(data, mode, TAR_UID, TAR_GID);
                file_size = 0;
                break;
            case TAR_FIFOTYPE: break;
            default:
                printf("%s\n", (char *)data);
                mode = parse_mode(data + TAR_OCTAL_OFF);
                for (int i = 0; i < 100; i++) path_name[i] = data[0 + i];
                file = create_file(data, mode, TAR_UID, TAR_GID);
                chown(path_name, TAR_UID, TAR_GID);
                chmod(path_name, mode);
                break;
        }
        
        while (file_size > 0) {
            bytes_read = fread(data, 1, 512, input);
            if (bytes_read < 512) return -1;
            if (file_size < 512) bytes_read = file_size;
            if (file != NULL) {
                if (fwrite(data, 1, bytes_read, file) != bytes_read) {
                    fclose(file);
                    file = NULL;
                }
            }
            file_size -= bytes_read;
        }
        
        if (file != NULL) {
            fclose(file);
            file = NULL;
        }
    }
    return 0;
}

void lzfse_release(lzfse_ctx_t *ctx) {
    if (ctx == NULL) return;
    if (ctx->stream_loaded) compression_stream_destroy(&ctx->stream);

    if (ctx->input_fd >= 0) close(ctx->input_fd);
    if (ctx->input_data != NULL) munmap(ctx->input_data, ctx->input_size);
    if (ctx->input_path != NULL) free(ctx->input_path);
    
    if (ctx->output_fd >= 0) close(ctx->output_fd);
    if (ctx->output_data != NULL) free(ctx->output_data);

    if (ctx->output_path != NULL) {
        if (ctx->status != COMPRESSION_STATUS_END) unlink(ctx->output_path);
        free(ctx->output_path);
    }
    free(ctx);
}

lzfse_ctx_t *lzfse_load(const char *input, const char *output) {
    if (input == NULL || output == NULL) return NULL;
    lzfse_ctx_t *ctx = calloc(1, sizeof(lzfse_ctx_t));
    if (ctx == NULL) goto err;
    
    if ((ctx->input_path = strdup(input)) == NULL) goto err;
    if ((ctx->input_fd = open(ctx->input_path, O_RDONLY)) < 0) goto err;
    
    struct stat st = {0};
    fstat(ctx->input_fd, &st);
    ctx->input_size = (uint32_t)st.st_size;
    if ((ctx->input_data = mmap(NULL, ctx->input_size, PROT_READ, MAP_PRIVATE, ctx->input_fd, 0)) == MAP_FAILED) goto err;

    if ((ctx->output_path = strdup(output)) == NULL) goto err;
    unlink(ctx->output_path);
    sync();
    
    ctx->output_size = 0x1000;
    if ((ctx->output_data = calloc(1, ctx->output_size)) == NULL) goto err;
    if ((ctx->output_fd = open(ctx->output_path, O_RDWR | O_CREAT, 0777)) < 0) goto err;
    
    ctx->status = compression_stream_init(&ctx->stream, COMPRESSION_STREAM_DECODE, COMPRESSION_LZFSE);
    if (ctx->status != COMPRESSION_STATUS_OK) goto err;
    ctx->stream_loaded = true;
    
    ctx->stream.src_ptr = ctx->input_data;
    ctx->stream.src_size = ctx->input_size;
    return ctx;

err:
    lzfse_release(ctx);
    return NULL;
}

int lzfse_decode(lzfse_ctx_t *ctx) {
    if (ctx == NULL) return -1;
    ctx->status = COMPRESSION_STATUS_OK;
    
    while (ctx->status == COMPRESSION_STATUS_OK) {
        ctx->stream.dst_ptr = ctx->output_data;
        ctx->stream.dst_size = ctx->output_size;
        
        ctx->status = compression_stream_process(&ctx->stream, 0);
        uint32_t size = ctx->output_size - (uint32_t)ctx->stream.dst_size;
        if (size > 0) write(ctx->output_fd, ctx->output_data, size);
    }
    return (ctx->status == COMPRESSION_STATUS_END) ? 0 : -1;
}

int extract_lzfse_tar(const char *input, const char *output) {
    lzfse_ctx_t *ctx = lzfse_load(input, "/tmp/decode.tar");
    if (ctx == NULL) return -1;

    int status = lzfse_decode(ctx);
    lzfse_release(ctx);
    if (status != 0) return -1;
    
    FILE *file = fopen("/tmp/decode.tar", "rb");
    if (file == NULL) return -1;

    status = extract_tar(file, output);
    fclose(file);
    
    unlink("/tmp/decode.tar");
    return status;
}
