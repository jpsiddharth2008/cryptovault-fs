#include "encfs.h"
#include "crypto.h"
#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ===== PERSON 2: metadata & directory operations ===== */

static int encfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (lstat(backing, st) == -1) {
        return -errno;
    }

    if (S_ISREG(st->st_mode)) {
        if (st->st_size >= (off_t)CRYPTO_OVERHEAD) {
            st->st_size -= CRYPTO_OVERHEAD;
        } else {
            st->st_size = 0;
        }
    }
    return 0;
}

static int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char backing[PATH_MAX];
    get_backing_path(backing, path);

    DIR *dp = opendir(backing);
    if (!dp) {
        return -errno;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (filler(buf, de->d_name, NULL, 0, 0) != 0) {
            break;
        }
    }

    closedir(dp);
    return 0;
}

static int encfs_mkdir(const char *path, mode_t mode) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (mkdir(backing, mode) == -1) {
        return -errno;
    }
    return 0;
}

static int encfs_rmdir(const char *path) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (rmdir(backing) == -1) {
        return -errno;
    }
    return 0;
}

static int encfs_unlink(const char *path) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (unlink(backing) == -1) {
        return -errno;
    }
    return 0;
}

static int encfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (chmod(backing, mode) == -1) {
        return -errno;
    }
    return 0;
}

static int encfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void) fi;
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (lchown(backing, uid, gid) == -1) {
        return -errno;
    }
    return 0;
}

static int encfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (utimensat(AT_FDCWD, backing, tv, AT_SYMLINK_NOFOLLOW) == -1) {
        return -errno;
    }
    return 0;
}

/* ===== PERSON 3: file handles & read ===== */

static int encfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    int fd = open(backing, O_RDWR | O_CREAT | O_TRUNC, mode);
    if (fd == -1) {
        return -errno;
    }
    fi->fh = fd;
    return 0;
}

static int encfs_open(const char *path, struct fuse_file_info *fi) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    int flags = fi->flags;
    // Translate write-only to read-write since we need to read & decrypt existing content for partial writes
    if ((flags & O_ACCMODE) == O_WRONLY) {
        flags = (flags & ~O_ACCMODE) | O_RDWR;
    }

    int fd = open(backing, flags);
    if (fd == -1) {
        return -errno;
    }
    fi->fh = fd;
    return 0;
}

static int encfs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    if (fi->fh != 0) {
        close(fi->fh);
    }
    return 0;
}

static int encfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) path;
    struct stat st;
    if (fstat(fi->fh, &st) == -1) {
        return -errno;
    }

    if (st.st_size <= (off_t)CRYPTO_OVERHEAD) {
        return 0; // Empty file or headers only
    }

    size_t backing_len = st.st_size;
    unsigned char *backing_data = malloc(backing_len);
    if (!backing_data) {
        return -ENOMEM;
    }

    if (pread(fi->fh, backing_data, backing_len, 0) == -1) {
        int err = errno;
        free(backing_data);
        return -err;
    }

    size_t plaintext_len = backing_len - CRYPTO_OVERHEAD;
    unsigned char *plaintext = malloc(plaintext_len);
    if (!plaintext) {
        free(backing_data);
        return -ENOMEM;
    }

    if (encfs_decrypt(backing_data, backing_len, plaintext, ENCFS_CTX->key) != 0) {
        free(backing_data);
        free(plaintext);
        return -EIO;
    }
    free(backing_data);

    if (offset >= (off_t)plaintext_len) {
        free(plaintext);
        return 0;
    }

    size_t bytes_to_copy = size;
    if (offset + size > plaintext_len) {
        bytes_to_copy = plaintext_len - offset;
    }

    memcpy(buf, plaintext + offset, bytes_to_copy);
    free(plaintext);

    return bytes_to_copy;
}

/* ===== PERSON 4: write & truncate ===== */

static int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void) path;
    struct stat st;
    if (fstat(fi->fh, &st) == -1) {
        return -errno;
    }

    size_t old_backing_len = st.st_size;
    size_t old_plaintext_len = 0;
    unsigned char *old_plaintext = NULL;

    if (old_backing_len > CRYPTO_OVERHEAD) {
        old_plaintext_len = old_backing_len - CRYPTO_OVERHEAD;
        unsigned char *old_backing_data = malloc(old_backing_len);
        if (!old_backing_data) {
            return -ENOMEM;
        }

        if (pread(fi->fh, old_backing_data, old_backing_len, 0) == -1) {
            int err = errno;
            free(old_backing_data);
            return -err;
        }

        old_plaintext = malloc(old_plaintext_len);
        if (!old_plaintext) {
            free(old_backing_data);
            return -ENOMEM;
        }

        if (encfs_decrypt(old_backing_data, old_backing_len, old_plaintext, ENCFS_CTX->key) != 0) {
            free(old_backing_data);
            free(old_plaintext);
            return -EIO;
        }
        free(old_backing_data);
    }

    size_t new_plaintext_len = old_plaintext_len;
    if (offset + size > new_plaintext_len) {
        new_plaintext_len = offset + size;
    }

    unsigned char *new_plaintext = malloc(new_plaintext_len);
    if (!new_plaintext) {
        free(old_plaintext);
        return -ENOMEM;
    }

    if (old_plaintext_len > 0) {
        memcpy(new_plaintext, old_plaintext, old_plaintext_len);
    }

    if ((size_t)offset > old_plaintext_len) {
        memset(new_plaintext + old_plaintext_len, 0, offset - old_plaintext_len);
    }

    memcpy(new_plaintext + offset, buf, size);
    free(old_plaintext);

    size_t new_backing_len = new_plaintext_len + CRYPTO_OVERHEAD;
    unsigned char *new_backing_data = malloc(new_backing_len);
    if (!new_backing_data) {
        free(new_plaintext);
        return -ENOMEM;
    }

    if (encfs_encrypt(new_plaintext, new_plaintext_len, new_backing_data, ENCFS_CTX->key) != 0) {
        free(new_plaintext);
        free(new_backing_data);
        return -EIO;
    }
    free(new_plaintext);

    if (ftruncate(fi->fh, new_backing_len) == -1) {
        int err = errno;
        free(new_backing_data);
        return -err;
    }

    if (pwrite(fi->fh, new_backing_data, new_backing_len, 0) == -1) {
        int err = errno;
        free(new_backing_data);
        return -err;
    }

    free(new_backing_data);
    return size;
}

static int encfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    int fd = -1;
    if (fi && fi->fh != 0) {
        fd = fi->fh;
    } else {
        fd = open(backing, O_RDWR);
        if (fd == -1) {
            return -errno;
        }
    }

    int ret = 0;
    if (size == 0) {
        if (ftruncate(fd, 0) == -1) {
            ret = -errno;
        }
    } else {
        struct stat st;
        if (fstat(fd, &st) == -1) {
            ret = -errno;
            goto out;
        }

        size_t old_backing_len = st.st_size;
        size_t old_plaintext_len = 0;
        unsigned char *old_plaintext = NULL;

        if (old_backing_len > CRYPTO_OVERHEAD) {
            old_plaintext_len = old_backing_len - CRYPTO_OVERHEAD;
            unsigned char *old_backing_data = malloc(old_backing_len);
            if (!old_backing_data) {
                ret = -ENOMEM;
                goto out;
            }

            if (pread(fd, old_backing_data, old_backing_len, 0) == -1) {
                ret = -errno;
                free(old_backing_data);
                goto out;
            }

            old_plaintext = malloc(old_plaintext_len);
            if (!old_plaintext) {
                ret = -ENOMEM;
                free(old_backing_data);
                goto out;
            }

            if (encfs_decrypt(old_backing_data, old_backing_len, old_plaintext, ENCFS_CTX->key) != 0) {
                ret = -EIO;
                free(old_backing_data);
                free(old_plaintext);
                goto out;
            }
            free(old_backing_data);
        }

        size_t new_plaintext_len = size;
        unsigned char *new_plaintext = malloc(new_plaintext_len);
        if (!new_plaintext) {
            free(old_plaintext);
            ret = -ENOMEM;
            goto out;
        }

        size_t to_copy = (old_plaintext_len < new_plaintext_len) ? old_plaintext_len : new_plaintext_len;
        if (to_copy > 0) {
            memcpy(new_plaintext, old_plaintext, to_copy);
        }

        if (new_plaintext_len > old_plaintext_len) {
            memset(new_plaintext + old_plaintext_len, 0, new_plaintext_len - old_plaintext_len);
        }

        free(old_plaintext);

        size_t new_backing_len = new_plaintext_len + CRYPTO_OVERHEAD;
        unsigned char *new_backing_data = malloc(new_backing_len);
        if (!new_backing_data) {
            free(new_plaintext);
            ret = -ENOMEM;
            goto out;
        }

        if (encfs_encrypt(new_plaintext, new_plaintext_len, new_backing_data, ENCFS_CTX->key) != 0) {
            free(new_plaintext);
            free(new_backing_data);
            ret = -EIO;
            goto out;
        }
        free(new_plaintext);

        if (ftruncate(fd, new_backing_len) == -1) {
            ret = -errno;
            free(new_backing_data);
            goto out;
        }

        if (pwrite(fd, new_backing_data, new_backing_len, 0) == -1) {
            ret = -errno;
            free(new_backing_data);
            goto out;
        }

        free(new_backing_data);
    }

out:
    if (!fi || fi->fh == 0) {
        close(fd);
    }
    return ret;
}

/* ===== Wiring ===== */

struct fuse_operations encfs_oper = {
    .getattr  = encfs_getattr,
    .readdir  = encfs_readdir,
    .mkdir    = encfs_mkdir,
    .rmdir    = encfs_rmdir,
    .unlink   = encfs_unlink,
    .create   = encfs_create,
    .open     = encfs_open,
    .release  = encfs_release,
    .read     = encfs_read,
    .write    = encfs_write,
    .truncate = encfs_truncate,
    .chmod    = encfs_chmod,
    .chown    = encfs_chown,
    .utimens  = encfs_utimens,
};
