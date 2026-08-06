#include "vaultfs.h"
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

void get_backing_path(char *dest, const char *path) {
    snprintf(dest, PATH_MAX, "%s%s", VAULT_CTX->backing_path, path);
}

static int vault_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    if (lstat(bpath, st) == -1) {
        return -errno;
    }

    // Adjust reported plaintext size for regular files
    if (S_ISREG(st->st_mode)) {
        if (st->st_size > CRYPTO_OVERHEAD) {
            st->st_size -= CRYPTO_OVERHEAD;
        } else {
            st->st_size = 0;
        }
    }
    return 0;
}

static int vault_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    DIR *dp = opendir(bpath);
    if (dp == NULL) {
        return -errno;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) {
            break;
        }
    }

    closedir(dp);
    return 0;
}

static int vault_mkdir(const char *path, mode_t mode) {
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    if (mkdir(bpath, mode) == -1) {
        return -errno;
    }
    return 0;
}

static int vault_rmdir(const char *path) {
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    if (rmdir(bpath) == -1) {
        return -errno;
    }
    return 0;
}

static int vault_unlink(const char *path) {
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    if (unlink(bpath) == -1) {
        return -errno;
    }
    return 0;
}

static int vault_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    int fd = open(bpath, fi->flags, mode);
    if (fd == -1) {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int vault_open(const char *path, struct fuse_file_info *fi) {
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    int fd = open(bpath, fi->flags);
    if (fd == -1) {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int vault_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);
    return 0;
}

static int vault_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    int fd = open(bpath, O_RDONLY);
    if (fd == -1) {
        return -errno;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -errno;
    }

    if (st.st_size == 0) {
        close(fd);
        return 0;
    }

    if (st.st_size < CRYPTO_OVERHEAD) {
        close(fd);
        return -EIO;
    }

    unsigned char *backing_buf = malloc(st.st_size);
    if (!backing_buf) {
        close(fd);
        return -ENOMEM;
    }

    ssize_t bytes_read = pread(fd, backing_buf, st.st_size, 0);
    close(fd);

    if (bytes_read != st.st_size) {
        free(backing_buf);
        return -EIO;
    }

    size_t plaintext_len = st.st_size - CRYPTO_OVERHEAD;
    unsigned char *plaintext_buf = malloc(plaintext_len);
    if (!plaintext_buf) {
        free(backing_buf);
        return -ENOMEM;
    }

    if (vault_decrypt(backing_buf, st.st_size, plaintext_buf, VAULT_CTX->key) != 0) {
        free(backing_buf);
        free(plaintext_buf);
        return -EIO;
    }

    free(backing_buf);

    if (offset >= (off_t)plaintext_len) {
        free(plaintext_buf);
        return 0;
    }

    size_t to_copy = size;
    if (offset + size > plaintext_len) {
        to_copy = plaintext_len - offset;
    }

    memcpy(buf, plaintext_buf + offset, to_copy);
    free(plaintext_buf);

    return to_copy;
}

static int vault_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    int fd = open(bpath, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        return -errno;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -errno;
    }

    unsigned char *plaintext_buf = NULL;
    size_t plaintext_len = 0;

    if (st.st_size >= CRYPTO_OVERHEAD) {
        unsigned char *backing_buf = malloc(st.st_size);
        if (!backing_buf) {
            close(fd);
            return -ENOMEM;
        }

        if (pread(fd, backing_buf, st.st_size, 0) != st.st_size) {
            free(backing_buf);
            close(fd);
            return -EIO;
        }

        plaintext_len = st.st_size - CRYPTO_OVERHEAD;
        plaintext_buf = malloc(plaintext_len);
        if (!plaintext_buf) {
            free(backing_buf);
            close(fd);
            return -ENOMEM;
        }

        if (vault_decrypt(backing_buf, st.st_size, plaintext_buf, VAULT_CTX->key) != 0) {
            free(backing_buf);
            free(plaintext_buf);
            close(fd);
            return -EIO;
        }
        free(backing_buf);
    }

    size_t new_len = plaintext_len;
    if (offset + size > new_len) {
        new_len = offset + size;
    }

    unsigned char *new_plaintext = realloc(plaintext_buf, new_len);
    if (!new_plaintext) {
        free(plaintext_buf);
        close(fd);
        return -ENOMEM;
    }
    plaintext_buf = new_plaintext;

    if (offset > (off_t)plaintext_len) {
        memset(plaintext_buf + plaintext_len, 0, offset - plaintext_len);
    }

    memcpy(plaintext_buf + offset, buf, size);

    size_t backing_len = new_len + CRYPTO_OVERHEAD;
    unsigned char *backing_buf = malloc(backing_len);
    if (!backing_buf) {
        free(plaintext_buf);
        close(fd);
        return -ENOMEM;
    }

    if (vault_encrypt(plaintext_buf, new_len, backing_buf, VAULT_CTX->key) != 0) {
        free(plaintext_buf);
        free(backing_buf);
        close(fd);
        return -EIO;
    }

    free(plaintext_buf);

    if (ftruncate(fd, 0) == -1) {
        free(backing_buf);
        close(fd);
        return -errno;
    }

    if (pwrite(fd, backing_buf, backing_len, 0) != (ssize_t)backing_len) {
        free(backing_buf);
        close(fd);
        return -EIO;
    }

    free(backing_buf);
    close(fd);

    return size;
}

static int vault_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);

    if (size == 0) {
        if (truncate(bpath, 0) == -1) {
            return -errno;
        }
        return 0;
    }

    int fd = open(bpath, O_RDWR);
    if (fd == -1) {
        return -errno;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -errno;
    }

    unsigned char *plaintext_buf = NULL;
    size_t plaintext_len = 0;

    if (st.st_size >= CRYPTO_OVERHEAD) {
        unsigned char *backing_buf = malloc(st.st_size);
        if (!backing_buf) {
            close(fd);
            return -ENOMEM;
        }

        if (pread(fd, backing_buf, st.st_size, 0) != st.st_size) {
            free(backing_buf);
            close(fd);
            return -EIO;
        }

        plaintext_len = st.st_size - CRYPTO_OVERHEAD;
        plaintext_buf = malloc(plaintext_len);
        if (!plaintext_buf) {
            free(backing_buf);
            close(fd);
            return -ENOMEM;
        }

        if (vault_decrypt(backing_buf, st.st_size, plaintext_buf, VAULT_CTX->key) != 0) {
            free(backing_buf);
            free(plaintext_buf);
            close(fd);
            return -EIO;
        }
        free(backing_buf);
    }

    unsigned char *new_plaintext = realloc(plaintext_buf, size);
    if (!new_plaintext) {
        free(plaintext_buf);
        close(fd);
        return -ENOMEM;
    }
    plaintext_buf = new_plaintext;

    if (size > (off_t)plaintext_len) {
        memset(plaintext_buf + plaintext_len, 0, size - plaintext_len);
    }

    size_t backing_len = size + CRYPTO_OVERHEAD;
    unsigned char *backing_buf = malloc(backing_len);
    if (!backing_buf) {
        free(plaintext_buf);
        close(fd);
        return -ENOMEM;
    }

    if (vault_encrypt(plaintext_buf, size, backing_buf, VAULT_CTX->key) != 0) {
        free(plaintext_buf);
        free(backing_buf);
        close(fd);
        return -EIO;
    }
    free(plaintext_buf);

    if (ftruncate(fd, 0) == -1) {
        free(backing_buf);
        close(fd);
        return -errno;
    }

    if (pwrite(fd, backing_buf, backing_len, 0) != (ssize_t)backing_len) {
        free(backing_buf);
        close(fd);
        return -EIO;
    }

    free(backing_buf);
    close(fd);
    return 0;
}

static int vault_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);
    if (chmod(bpath, mode) == -1) {
        return -errno;
    }
    return 0;
}

static int vault_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);
    if (lchown(bpath, uid, gid) == -1) {
        return -errno;
    }
    return 0;
}

static int vault_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char bpath[PATH_MAX];
    get_backing_path(bpath, path);
    if (utimensat(AT_FDCWD, bpath, tv, AT_SYMLINK_NOFOLLOW) == -1) {
        return -errno;
    }
    return 0;
}

struct fuse_operations vault_oper = {
    .getattr  = vault_getattr,
    .readdir  = vault_readdir,
    .mkdir    = vault_mkdir,
    .rmdir    = vault_rmdir,
    .unlink   = vault_unlink,
    .create   = vault_create,
    .open     = vault_open,
    .release  = vault_release,
    .read     = vault_read,
    .write    = vault_write,
    .truncate = vault_truncate,
    .chmod    = vault_chmod,
    .chown    = vault_chown,
    .utimens  = vault_utimens,
};
