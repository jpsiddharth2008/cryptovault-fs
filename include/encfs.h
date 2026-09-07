#ifndef ENCFS_H
#define ENCFS_H

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <limits.h>

/* Shared state every FUSE callback needs. A pointer to one of these is
 * handed to fuse_main() as private_data; retrieve it via ENCFS_CTX. */
struct encfs_context {
    char backing_path[PATH_MAX];   /* absolute path to the real (hidden) storage dir */
    unsigned char key[32];         /* derived symmetric encryption key */
};

#define ENCFS_CTX ((struct encfs_context *)fuse_get_context()->private_data)

/* The populated FUSE operations table, defined in fuse_ops.c */
extern struct fuse_operations encfs_oper;

#endif /* ENCFS_H */
