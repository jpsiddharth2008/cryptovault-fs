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

/* ===== Metadata & directory operations (issues #6-#9) ===== */

/* TODO (issue #6): stat the backing file, then correct st->st_size for
 * CRYPTO_OVERHEAD (the encrypted file is bigger than the plaintext by
 * NONCE_SIZE + MAC_SIZE bytes) -- clamp to 0 rather than going negative. */
static int encfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    char backing_path[PATH_MAX];
    get_backing_path(backing_path, path);

    if (lstat(backing_path, st) == -1) {
        return -errno;
    }

    if (S_ISREG(st->st_mode)) {
        if (st->st_size >= (off_t) CRYPTO_OVERHEAD) {
            st->st_size -= CRYPTO_OVERHEAD;
        } else {
            st->st_size = 0;
        }
    }

    return 0;
}

/* TODO (issue #7): open the backing directory, loop readdir() over it,
 * report each entry via filler(). Pure passthrough, no crypto involved. */
static int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char backing_path[PATH_MAX];
    get_backing_path(backing_path, path);

    DIR *dp = opendir(backing_path);
    if (dp == NULL) {
        return -errno;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (filler(buf, entry->d_name, NULL, 0, 0) != 0) {
            closedir(dp);
            return -ENOMEM;
        }
    }

    closedir(dp);
    return 0;
}

/* Implements encfs_mkdir: creates a directory in the backing store. */
static int encfs_mkdir(const char *path, mode_t mode) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (mkdir(backing, mode) == -1)
        return -errno;

    return 0;
}

/* Implements encfs_rmdir: removes an empty directory from the backing store. */
static int encfs_rmdir(const char *path) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (rmdir(backing) == -1)
        return -errno;

    return 0;
}

/* Implements encfs_unlink: deletes a file from the backing store. */
static int encfs_unlink(const char *path) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    if (unlink(backing) == -1)
        return -errno;

    return 0;
}

static int encfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char backing_path[PATH_MAX];
    get_backing_path(backing_path, path);

    if (chmod(backing_path, mode) != 0) {
        return -errno;
    }
    return 0;
}

/* lchown, not chown -- if backing_path is a symlink, we want to change the
 * link's own ownership rather than following it. */
static int encfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void) fi;
    char backing_path[PATH_MAX];
    get_backing_path(backing_path, path);

    if (lchown(backing_path, uid, gid) != 0) {
        return -errno;
    }
    return 0;
}

static int encfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char backing_path[PATH_MAX];
    get_backing_path(backing_path, path);

    if (utimensat(AT_FDCWD, backing_path, tv, 0) != 0) {
        return -errno;
    }
    return 0;
}

/* ===== File handles & read (issues #10-#12) ===== */

/* Implements encfs_create: called when a brand-new file is being made.
 * Opens the backing file with O_CREAT|O_RDWR and stashes the fd in fi->fh
 * so that later read/write/release calls can retrieve it without re-opening. */
static int encfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    int fd = open(backing, O_CREAT | O_RDWR, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

/* Implements encfs_open: called when an existing file is being opened.
 * Always opens O_RDWR — even if the caller only asked for O_WRONLY —
 * because every write must first read+decrypt the current file contents. */
static int encfs_open(const char *path, struct fuse_file_info *fi) {
    char backing[PATH_MAX];
    get_backing_path(backing, path);

    int fd = open(backing, O_RDWR);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int encfs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);
    return 0;
}

static int encfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) path;

    struct stat st;
    if (fstat(fi->fh, &st) != 0) {
        return -errno;
    }

    /* A freshly created file has no encrypted blob yet: empty plaintext. */
    if (st.st_size == 0) {
        return 0;
    }
    if (st.st_size < (off_t) CRYPTO_OVERHEAD) {
        return -EIO;
    }

    size_t backing_len = (size_t) st.st_size;
    size_t plaintext_len = backing_len - CRYPTO_OVERHEAD;

    unsigned char *backing = malloc(backing_len);
    unsigned char *plaintext = malloc(plaintext_len > 0 ? plaintext_len : 1);
    if (backing == NULL || plaintext == NULL) {
        free(backing);
        free(plaintext);
        return -ENOMEM;
    }

    size_t got = 0;
    while (got < backing_len) {
        ssize_t n = pread(fi->fh, backing + got, backing_len - got, (off_t) got);
        if (n < 0) {
            int err = errno;
            free(backing);
            free(plaintext);
            return -err;
        }
        if (n == 0) {
            break;
        }
        got += (size_t) n;
    }

    int rc = (got == backing_len)
        ? encfs_decrypt(backing, backing_len, plaintext, ENCFS_CTX->key)
        : -1;
    free(backing);
    if (rc != 0) {
        free(plaintext);
        return -EIO;
    }

    int copied = 0;
    if (offset >= 0 && (size_t) offset < plaintext_len) {
        size_t avail = plaintext_len - (size_t) offset;
        size_t n = size < avail ? size : avail;
        memcpy(buf, plaintext + offset, n);
        copied = (int) n;
    }

    free(plaintext);
    return copied;
}

/* ===== Write & truncate (issues #13-#14) ===== */

/* TODO (issue #13): decrypt the whole current file -> apply this write's
 * edit in memory at `offset` (zero-pad any gap for a sparse write past
 * current EOF) -> re-encrypt the WHOLE result with a FRESH nonce ->
 * overwrite the backing file entirely. Return `size` on success. */
/* Every write re-encrypts the WHOLE file: decrypt what's currently on
 * disk, apply this write's edit in plaintext memory, re-encrypt the
 * entire result with a fresh nonce, then overwrite the backing file. */
static int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void) path;
    int fd = (int) fi->fh;

    /* How big is the encrypted blob currently sitting on disk? */
    struct stat st;
    if (fstat(fd, &st) == -1) {
        return -errno;
    }
    size_t backing_len = (size_t) st.st_size;

    /* Read + decrypt the file's CURRENT full contents (empty file -> empty plaintext). */
    size_t old_plain_len = 0;
    unsigned char *old_plain = NULL;

    if (backing_len > 0) {
        unsigned char *backing_buf = malloc(backing_len);
        if (backing_buf == NULL) {
            return -ENOMEM;
        }
        if (pread(fd, backing_buf, backing_len, 0) != (ssize_t) backing_len) {
            free(backing_buf);
            return -EIO;
        }

        old_plain_len = backing_len - CRYPTO_OVERHEAD;
        old_plain = malloc(old_plain_len);
        if (old_plain == NULL) {
            free(backing_buf);
            return -ENOMEM;
        }
        if (encfs_decrypt(backing_buf, backing_len, old_plain, ENCFS_CTX->key) != 0) {
            free(backing_buf);
            free(old_plain);
            return -EIO;
        }
        free(backing_buf);
    }

    /* New plaintext length: at least offset+size, or the old length if that's bigger. */
    size_t new_len = (size_t) offset + size;
    if (old_plain_len > new_len) {
        new_len = old_plain_len;
    }

    /* calloc zero-fills the buffer for us -- this handles the "gap" case
     * (a sparse write past the old end of file) for free. */
    unsigned char *new_plain = calloc(1, new_len > 0 ? new_len : 1);
    if (new_plain == NULL) {
        free(old_plain);
        return -ENOMEM;
    }

    if (old_plain_len > 0) {
        memcpy(new_plain, old_plain, old_plain_len);
    }
    free(old_plain);

    /* Drop the caller's new bytes in at the requested offset. */
    memcpy(new_plain + offset, buf, size);

    /* Re-encrypt the ENTIRE new plaintext with a brand-new nonce. */
    size_t new_backing_len = new_len + CRYPTO_OVERHEAD;
    unsigned char *new_backing = malloc(new_backing_len);
    if (new_backing == NULL) {
        free(new_plain);
        return -ENOMEM;
    }
    if (encfs_encrypt(new_plain, new_len, new_backing, ENCFS_CTX->key) != 0) {
        free(new_plain);
        free(new_backing);
        return -EIO;
    }
    free(new_plain);

    /* Overwrite the backing file entirely, then trim off any leftover old bytes. */
    if (pwrite(fd, new_backing, new_backing_len, 0) != (ssize_t) new_backing_len) {
        free(new_backing);
        return -EIO;
    }
    free(new_backing);

    if (ftruncate(fd, (off_t) new_backing_len) == -1) {
        return -errno;
    }

    return (int) size;
}

/* TODO (issue #14): same decrypt -> modify -> re-encrypt -> rewrite pattern
 * as write, but the modification is padding with zeros (growing) or
 * cutting off (shrinking) to reach exactly `size` bytes. Special-case
 * size == 0 -- no need to decrypt anything first in that case. */
static int encfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) path;
    (void) size;
    (void) fi;
    return -ENOSYS;
}

/* ===== Wiring (issue #16) =====
 *
 * TODO: once the callbacks above are implemented, populate this table,
 * e.g. .getattr = encfs_getattr, .read = encfs_read, etc. Until every
 * field is wired up, FUSE has no way to route a syscall to your function
 * even if that function is fully correct. */
struct fuse_operations encfs_oper = {0};
