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
    (void) path;
    (void) st;
    (void) fi;
    return -ENOSYS;
}

/* TODO (issue #7): open the backing directory, loop readdir() over it,
 * report each entry via filler(). Pure passthrough, no crypto involved. */
static int encfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    (void) path;
    (void) buf;
    (void) filler;
    (void) offset;
    (void) fi;
    (void) flags;
    return -ENOSYS;
}

/* TODO (issue #8): translate path, call mkdir() on the backing path. */
static int encfs_mkdir(const char *path, mode_t mode) {
    (void) path;
    (void) mode;
    return -ENOSYS;
}

/* TODO (issue #8): translate path, call rmdir() on the backing path. */
static int encfs_rmdir(const char *path) {
    (void) path;
    return -ENOSYS;
}

/* TODO (issue #8): translate path, call unlink() on the backing path. */
static int encfs_unlink(const char *path) {
    (void) path;
    return -ENOSYS;
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

/* TODO (issue #10): translate path, open(backing, O_CREAT|O_RDWR, mode),
 * stash the resulting fd in fi->fh for read/write/release to reuse. */
static int encfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) path;
    (void) mode;
    (void) fi;
    return -ENOSYS;
}

/* TODO (issue #10): translate path, open(backing, O_RDWR), stash the fd
 * in fi->fh. Note: an incoming O_WRONLY should be upgraded to O_RDWR,
 * since writes need to read+decrypt the existing content first. */
static int encfs_open(const char *path, struct fuse_file_info *fi) {
    (void) path;
    (void) fi;
    return -ENOSYS;
}

/* TODO (issue #11): close the fd stored in fi->fh by create/open. */
static int encfs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    (void) fi;
    return -ENOSYS;
}

/* TODO (issue #12): read the FULL encrypted blob from fi->fh, decrypt the
 * whole thing (encfs_decrypt), then copy out just [offset, offset+size)
 * of the resulting plaintext into buf. Handle offset at/past EOF, and
 * offset+size past EOF. Return the number of bytes actually copied. */
static int encfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) path;
    (void) buf;
    (void) size;
    (void) offset;
    (void) fi;
    return -ENOSYS;
}

/* ===== Write & truncate (issues #13-#14) ===== */

/* TODO (issue #13): decrypt the whole current file -> apply this write's
 * edit in memory at `offset` (zero-pad any gap for a sparse write past
 * current EOF) -> re-encrypt the WHOLE result with a FRESH nonce ->
 * overwrite the backing file entirely. Return `size` on success. */
static int encfs_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void) path;
    (void) buf;
    (void) size;
    (void) offset;
    (void) fi;
    return -ENOSYS;
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
